unit Prefab;

interface

uses Classes, J_Level, geometry, lev_utils, FileOperations, Files,
  misc_utils, sysUtils;

Var
  Prefabs: TStringList;

Function GetPreFab(n: integer): TJKSector;
Procedure LoadPrefabs(const fname: string);
Procedure SavePrefabs(const fname: string);
Procedure AddPrefab(const pname: string; s: TJKSector);
procedure DeletePrefab(n: integer);

implementation

Function GetPreFab(n: integer): TJKSector;
begin
  Result := TJKSector(Prefabs.Objects[n]);
end;

Procedure LoadPrefabs(const fname: string);
var
  p, i: integer;
  t: TTextFile;
  s, st, w: string;

  Function ReadFmt(const fmt: string; v: array of pointer): Boolean;
  begin
    t.Readln(st);
    Result := SScanf(st, fmt, v);
  end;

  Procedure ReadS(Var s: string);
  begin
    t.Readln(s);
    RemoveComment(s);
  end;

  Function LoadSector: TJKSector;
  var
    sec: TJKSector;
    nRead, sf, nsf, vx, nv, nsv: integer;
    asec, asf: integer;
    v: TJKVertex;
    surf: TJKSurface;
  begin
    sec := Level.NewSector;
    Result := sec;
    try
      With sec do
      begin
        // TODO: parse version 2 with RGB colors for Ambient & ExtraLight, and IJIM light intensity, force etc...
        if not ReadFmt('%s %s %f %x %l %l %l %l %l %d', [@ColorMap, @Sound, @soundVolume,
          @Flags, @Ambient.a, @ExtraLight.a, @tint.r, @tint.g, @tint.b, @Layer]) then
          raise Exception.Create('Failed to parse template sector info');

        Ambient    := IntensityToColor(Ambient.a);
        ExtraLight := IntensityToColor(ExtraLight.a);
        if ColorMap = '-' then
          ColorMap := '';
        if Sound = '-' then
          Sound := '';

        ReadFmt('VXS %d', [@nv]);
        for vx := 0 to nv - 1 do
        begin
          v := sec.NewVertex;
          { Sec.Vertices.Add(v); }
          With v do
            ReadFmt('%f %f %f', [@x, @y, @z]);
        end;
        ReadFmt('SURFS %d', [@nsf]);
        for sf := 0 to nsf - 1 do
        begin
          surf := sec.NewSurface;
          sec.Surfaces.Add(surf);
          With surf do
          begin
            // TODO: parse version 2 with RGB colors for ExtraLight
            if not ReadFmt('%s %x %x %d %d %d %l %d %d %d %x',
              [@Material, @SurfFlags, @FaceFlags, @geo, @light, @tex,
              @ExtraLight.a, @nsv, @asec, @asf, @AdjoinFlags])
              then raise Exception.Create('Failed to parse template sector surface info');
            ExtraLight := IntensityToColor(ExtraLight.a);
            if Material = '-' then
              Material := '';
            { Num:=asec shl 16+(asf and $FFFF); }
            ReadS(st);
            p := 1;
            for vx := 0 to nsv - 1 do
            begin
              p := GetWord(st, p, w);
              ValInt(w, nv);
              AddVertex(sec.Vertices[nv]);
              With TXVertices[vx] do
              begin
                p := GetWord(st, p, w);
                ValSingle(w, u);
                p := GetWord(st, p, w);
                ValSingle(w, v);
                p := GetWord(st, p, w);
                ValSingle(w, color.a);
              end;
            end;
            { Load RGB }
            for vx := 0 to nsv - 1 do
              With TXVertices[vx] do
              begin
                p := GetWord(st, p, w);
                if w = '' then
                  break;
                ValSingle(w, color.r);
                p := GetWord(st, p, w);
                ValSingle(w, color.g);
                p := GetWord(st, p, w);
                ValSingle(w, color.b);
              end;

          end;
        end;
      end;
      sec.Renumber;
    except
      On E: Exception do
      begin
        PanMessageFmt(mt_error, 'Error loading %s at line %d: %s',
          [fname, t.curline, E.message]);
        sec.free;
        Result := nil;
      end;
    end;
  end;

var
  sec: TJKSector;
  sname: string;
begin
  for i := 1 to Prefabs.count - 1 do
    Prefabs.Objects[i].free;
  For i := Prefabs.count - 1 downto 1 do
    Prefabs.Delete(i);

  try
    t := TTextFile.CreateRead(OpenFileRead(fname, 0));
  except
    on E: Exception do
    begin
      PanMessage(mt_warning, E.message + ' ' + fname);
      exit;
    end;
  end;

  try

    while not t.eof do
    begin
      t.Readln(s);
      RemoveComment(s);
      p := GetWord(s, 1, w);
      if CompareText(w, 'SEC') = 0 then
      begin
        sname := Trim(Copy(s, p, length(s)));
        // TODO: add version parsing
        sec := LoadSector;
        if sec <> nil then
          Prefabs.AddObject(sname, sec);
      end;
    end;

  finally
    t.Fclose;
  end;

end;

procedure DeletePrefab(n: integer);
begin
  if n = 0 then
    exit;
  Prefabs.Objects[n].free();
  Prefabs.Delete(n);
end;

Procedure AddPrefab(const pname: string; s: TJKSector);
var
  newsec: TJKSector;
  nx, ny, nz: Tvector;
  cx, cy, cz: double;
  i: integer;
begin
  cx := 99999;
  cy := 99999;
  cz := 99999;

  for i := 0 to s.Vertices.count - 1 do
    With s.Vertices[i] do
    begin
      if x < cx then
        cx := x;
      if y < cy then
        cy := y;
      if z < cz then
        cz := z;
    end;

  if (cx = 99999) or (cy = 99999) or (cz = 99999) then
    exit;

  nx.SetCoords(1, 0, 0);
  ny.SetCoords(0, 1, 0);
  nz.SetCoords(0, 0, 1);
  newsec := Level.NewSector;
  DuplicateSector(s, newsec, cx, cy, cz, nx, ny, nz, -cx, -cy, -cz);
  Prefabs.AddObject(pname, newsec);
end;

Procedure SavePrefabs(const fname: string);
var
  i, sf, vx: integer;
  asf, asec: integer;
  st: string;
  t: Text;

  Procedure WriteFmt(const fmt: string; v: array of const);
  begin
    Writeln(t, Sprintf(fmt, v));
  end;

begin
  BackUpFile(fname);
  Assign(t, fname);
  Rewrite(t);

  for i := 1 to Prefabs.count - 1 do
    With TJKSector(Prefabs.Objects[i]) do
    begin
      // TODO: Add version no. 2 to the end of SEC line
      WriteFmt('SEC %s', [Prefabs[i]]);
      if ColorMap = '' then
        st := '-'
      else
        st := ColorMap;
      if Sound = '' then
        st := st + ' -'
      else
        st := st + ' ' + Sound;

      // TODO: Write Ambient & ExtraLIght as RGB, store IJIM light intensity, force etc ...
      WriteFmt('%s %.6f %x %.6f %.6f %.6f %.6f %.6f %d',
        [st, soundVolume, Flags, RgbToIntensity(Ambient), RgbToIntensity(ExtraLight), tint.r, tint.g, tint.b, Layer]);

      WriteFmt('VXS %d', [Vertices.count]);
      for vx := 0 to Vertices.count - 1 do
        With Vertices[vx] do
        begin
          WriteFmt('%.6f %.6f %.6f', [x, y, z]);
        end;
      WriteFmt('SURFS %d', [Surfaces.count]);
      for sf := 0 to Surfaces.count - 1 do
        With Surfaces[sf] do
        begin
          if adjoin <> nil then
          begin
            asf := adjoin.Num;
            asec := adjoin.Sector.Num;
          end
          else
          begin
            asf := -1;
            asec := -1;
          end;
          st := Material;
          if st = '' then
            st := '-';
          // TODO: WriteExtraLIght as RGB
          WriteFmt('%s %x %x %d %d %d %.6f %d %d %d %x %.4f %.4f',
            [st, SurfFlags, FaceFlags, geo, light, tex, RgbaToIntensity(ExtraLight),
            Vertices.count, asec, asf, AdjoinFlags, uscale, vscale]);
          st := '';
          for vx := 0 to Vertices.count - 1 do
            With TXVertices[vx] do
            begin
              st := Concat(st, Sprintf(' %d %.2f %.2f %.4f',
                [Vertices[vx].Num, u, v, color.a]));
            end;
          for vx := 0 to Vertices.count - 1 do
            With TXVertices[vx] do
            begin
              st := Concat(st, Sprintf(' %.4f %.4f %.4f', [color.r, color.g, color.b]));
            end;
          Writeln(t, st);
        end;

      Writeln(t, 'END');
      Writeln(t);
    end;
  close(t);
end;

Procedure InitList;
var
  js: TJKSector;
  z, x: Tvector;
begin
  Prefabs := TStringList.Create;
  js := Level.NewSector;
  z.SetCoords(0, 0, 1);
  x.SetCoords(1, 0, 0);
  CreateCubeSec(js, 0, 0, 0, z, x);
  Prefabs.AddObject('Cube', js);
end;

Initialization

InitList;

end.
