unit Containers;

interface
Uses Files, Classes, SysUtils;
Type

TGOBheader=packed record
            magic:array[0..3] of char;
	    index_ofs:longint;
	   end;

TNEntries=longint;

TGOBEntry=packed record
	 offs:longint;
	 size:longint;
	 name:array[0..12] of char;
	end;


 TLFDentry=packed record
	 tag:array[0..3] of char;
	 name:array[0..7] of char;
	 size:longint;
	end;


TFInfo=TFileInfo;

TGOBDirectory=class(TContainerFile)
 gh:TGobHeader;
 Procedure Refresh;override;
 Function GetContainerCreator(name:String):TContainerCreator;override;
end;

TFLDDirectory=class(TContainerFile)
 Procedure Refresh;override;
 Function GetContainerCreator(name:String):TContainerCreator;override;
end;

TGOBCreator=class(TContainerCreator)
 pes:TList;
 centry:integer;
 Procedure PrepareHeader(newfiles:TStringList);override;
 Procedure AddFile(F:TFile);override;
 Function ValidateName(name:String):String;override;
 Destructor Destroy;override;
end;

TLFDCreator=class(TContainerCreator)
 pes:TList;
 centry:integer;
 Procedure PrepareHeader(newfiles:TStringList);override;
 Procedure AddFile(F:TFile);override;
 Function ValidateName(name:String):String;override;
 Destructor Destroy;override;
private
 Function ValidateExt(e:string):String;
end;


{LAB Types declarations}

Type
TResType=array[0..3] of char;

TLABDirectory=class(TContainerFile)
 Procedure Refresh;override;
 Function GetContainerCreator(name:String):TContainerCreator;override;
end;

 TLABHeader=packed record
   Magic:array[0..3] of char; {'LABN'}
   Version:Longint; {65536}
   NFiles:Longint;
   NamePoolSize:Longint;
 end;

TLABEntry=packed record
 Nameoffset:longint; // from the beginning of the name pool
 offset:Longint;	//absolute from the beginning of the file
 size:longint;
 ResType:TResType;
end;

TLABCreator=class(TContainerCreator)
 pes:TList;
 centry:integer;
 lh:TLABHeader;
 Procedure PrepareHeader(newfiles:TStringList);override;
 Procedure AddFile(F:TFile);override;
 Function ValidateName(name:String):String;override;
 Destructor Destroy;override;
end;

Function GetLABResType(const Name:String):TResType;

{WAD decalarations}

Type
Wad_index=packed record
 magic:array[0..3] of char;
 nentries:longint;
 pdirectory: longint;
end;

TWADDirectory=class(TContainerFile)
 wh:Wad_index;
 Procedure Refresh;override;
end;

{GOB2 declarations}
TGOB2Header=record
 Magic:array[0..3] of char; {='GOB'#20}
 Long1,long2:longint; { $14, $C}
 NEntries:longint;
end;

TGOB2Entry=record
 Pos,size:longint;
 name:array[0..127] of char;
end;

TGOB2Directory=class(TContainerFile)
 gh:TGob2Header;
 Procedure Refresh;override;
 Function GetColWidth(n:Integer):Integer;override;
 Function GetContainerCreator(name:String):TContainerCreator;override;
end;

TGOB2Creator=class(TContainerCreator)
 pes:TList;
 centry:integer;
 Procedure PrepareHeader(newfiles:TStringList);override;
 Procedure AddFile(F:TFile);override;
 Function ValidateName(name:String):String;override;
 Destructor Destroy;override;
end;


implementation
Uses FileOperations;

Procedure TGOBCreator.PrepareHeader(newfiles:TStringList);
var i:Integer;
    pe:^TGOBEntry;
    aName:STring;
begin
 pes:=TList.Create;
 cf.FSeek(sizeof(TGOBHeader));
 for i:=0 to newfiles.count-1 do
 begin
  New(pe);
  aName:=ExtractName(Newfiles[i]);
  StrPCopy(Pe^.Name,ValidateName(aName));
  pes.Add(pe);
 end;
 centry:=0;
end;

Procedure TGOBCreator.AddFile(F:TFile);
begin
 with TgobEntry(pes[centry]^) do
 begin
  offs:=cf.Fpos;
  size:=f.Fsize;
 end;
 CopyFileData(F,CF,f.Fsize);
 inc(Centry);
end;

Function TGOBCreator.ValidateName(name:String):String;
var n,ext:String;
begin
 Ext:=ExtractExt(name);
 n:=Copy(Name,1,length(Name)-length(ext));
 if length(Ext)>4 then SetLength(Ext,4);
 if length(N)>8 then SetLength(N,8);
 Result:=N+ext;
end;

Destructor TGOBCreator.Destroy;
var i:integer;
    gh:TGobHeader;
    n:Longint;
begin
if pes<>nil then
begin
 gh.magic:='GOB'#10;
 gh.index_ofs:=cf.Fpos;
 cf.Fseek(0); cf.FWrite(gh,sizeof(gh));
 cf.FSeek(gh.index_ofs);
 n:=pes.count;
 cf.FWrite(n,sizeof(n));
 for i:=0 to pes.count-1 do
 begin
  cf.FWrite(TGOBEntry(pes[i]^),sizeof(TGOBEntry));
  Dispose(pes[i]);
 end;
 pes.Free;
end;
Inherited Destroy;
end;


Procedure TGOBDirectory.Refresh;
var Fi:TFInfo;
    i:integer;
    ge:TGobEntry;
    gn:TNEntries;
    f:TFile;
begin
 ClearIndex;
 f:=OpenFileRead(name,0);
Try
 F.FRead(gh,sizeof(gh));
 if gh.magic<>'GOB'#10 then raise Exception.Create(Name+' is not a GOB file');
 F.FSeek(gh.index_ofs);
 F.FRead(gn,sizeof(gn));
 for i:=0 to gn-1 do
 begin
  F.FRead(ge,sizeof(ge));
  fi:=TFInfo.Create;
  fi.offs:=ge.offs;
  fi.size:=ge.size;
  Files.AddObject(ge.name,fi);
 end;
Finally
 F.FClose;
end;
end;

Function TGOBDirectory.GetContainerCreator(name:String):TContainerCreator;
begin
 Result:=TGOBCreator.Create(Name);
end;

{Procedure TDirectoryFile.RenameFile(Fname,newname:TFileName);
var i:integer;
begin
 i:=Files.IndexOf(Fname);
 if i=-1 then exit;
 Files.Strings[i]:=NewName;
end;}

Procedure TFLDDirectory.Refresh;
var Le:TLFDEntry;
    N:array[0..12] of char;
    E:array[0..5] of char;
    fi:TFInfo;
    f:TFile;
begin
 ClearIndex;
 F:=OpenFileRead(Name,0);
 Try
 While (F.FPos<F.FSize) do
 begin
  F.FRead(le,sizeof(le));
  StrLCopy(n,le.name,sizeof(le.name));
  StrLCopy(e,le.tag,sizeof(le.tag));
  fi:=TFInfo.Create;
  Fi.offs:=F.FPos;
  fi.size:=le.size;
  Fi.offs:=F.Fpos;
  Files.AddObject(Concat(n,'.',e),fi);
  F.Fseek(F.Fpos+le.size);
 end;
Finally
 F.FClose;
end;
end;

Function TFLDDirectory.GetContainerCreator(name:String):TContainerCreator;
begin
 Result:=TLFDCreator.Create(Name);
end;

Procedure TLFDCreator.PrepareHeader(newfiles:TStringList);
var i:Integer;
    pe:^TLFDEntry;
    name,ext:string;
begin
 pes:=TList.Create;
 cf.FSeek(sizeof(TLFDEntry)*newfiles.count+sizeof(TLFDEntry));
 for i:=0 to newfiles.count-1 do
 begin
  New(pe);
  name:=ValidateName(ExtractName(Newfiles[i])); {always 4 letter extension}
  ext:=ExtractExt(name);
  Delete(Ext,1,1);
  SetLength(Name,length(Name)-5);
  StrMove(Pe^.Name,Pchar(Name),8);
  StrMove(pe^.tag,Pchar(Ext),4);
  pe^.size:=0;
  pes.Add(pe);
 end;
 centry:=0;
end;

Procedure TLFDCreator.AddFile(F:TFile);
begin
 TLFDEntry(Pes[centry]^).size:=f.Fsize;
 cf.FWrite(TLFDEntry(Pes[centry]^),sizeof(TLFDEntry));
 CopyFileData(f,cf,f.Fsize);
 inc(centry);
end;

Function TLFDCreator.ValidateName(name:String):String;
var n,ext:String;
begin
 Ext:=ExtractExt(name);
 n:=Copy(name,1,length(Name)-length(ext));
 Ext:=ValidateExt(ext);
 n:=LowerCase(n);
 if length(n)>8 then SetLength(n,8);
 Result:=n+ext;
end;

Destructor TLFDCreator.Destroy;
var i:integer;
    le:TLFDEntry;
begin
if pes<>nil then
begin
 cf.Fseek(0);
 le.name:='resource';
 le.tag:='RMAP';
 le.size:=pes.count*sizeof(TLFDEntry);
 cf.FWrite(le,sizeof(le));
 for i:=0 to pes.count-1 do
 begin
  cf.FWrite(TLFDEntry(pes[i]^),sizeof(TLFDEntry));
  Dispose(pes[i]);
 end;
 pes.Free;
end;
Inherited Destroy;
end;

Function TLFDCreator.ValidateExt(e:string):String;
begin
 Result:=UpperCase(e);
 if (result='') or (result='.') then Result:='.DATA'
 else if result='.TXT' then result:='.TEXT'
 else if result='.DLT' then result:='.DELT'
 else if result='.ANM' then result:='.ANIM'
 else if result='.FON' then result:='.FONT'
 else if result='.PLT' then result:='.PLTT'
 else if result='.GMD' then result:='.GMID'
 else if result='.VOC' then result:='.VOIC'
 else if result='.FLM' then result:='.FILM'
 else if length(result)<5 then
  while length(result)<5 do result:=result+'X';
end;

{LAB methods}
Type
TLABFileInfo=class(TFileInfo)
 resType:TResType;
end;

Procedure TLABDirectory.Refresh;
var f:TFile;
    Lh:TLabHeader;
    Le:TLabEntry;
    PNpool:pchar;
    Fi:TLABFileInfo;
    i:integer;
    s:string;
begin
 ClearIndex;
 f:=OpenFileRead(name,0);
Try
 F.FRead(lh,sizeof(lh));
 if (lh.magic<>'LABN') then raise Exception.Create(Name+' is not a LAB file');
 F.Fseek(lh.NFiles*sizeof(le)+sizeof(lh));
 GetMem(PNPool,lh.NamePoolSize);
 F.Fread(PNPool^,lh.NamePoolSize);
 F.Fseek(sizeof(lh));
 for i:=0 to lh.nFiles-1 do
 begin
  F.FRead(le,sizeof(le));
  fi:=TLABFileInfo.Create;
  fi.offs:=le.offset;
  fi.size:=le.size;
  fi.resType:=le.ResType;
  Files.AddObject((PNPool+le.NameOffset),fi);
 end;
Finally
 F.FClose;
 FreeMem(PnPool);
end;
end;

Function TLABDirectory.GetContainerCreator(name:String):TContainerCreator;
begin
 Result:=TLABCreator.Create(Name);
end;

Procedure TLABCreator.PrepareHeader(newfiles:TStringList);
var i:integer;
    buf:Array[0..255] of char;
    len:Integer;
    ple:^TLabEntry;
    aName:String;
begin
 pes:=TList.Create;
 Lh.magic:='LABN';
 lh.Version:=65536;
 lh.NFiles:=newfiles.count;
 lh.NamePoolSize:=0;
 cf.Fseek(sizeof(lh)+lh.NFiles*sizeof(TLABEntry));
 for i:=0 to newfiles.count-1 do
 begin
  aName:=ExtractName(newfiles[i]);
  StrPCopy(buf,aName);
  len:=Length(aName);
  cf.FWrite(buf,len+1);
  new(ple);
  with ple^ do
  begin
   NameOffset:=lh.NamePoolSize;
   size:=0;
   offset:=0;
   resType:=GetLABResType(aName);
  end;
  inc(lh.NamePoolSize,len+1);
  pes.Add(ple);
 end;
 Centry:=0;
end;

Function GetLABResType(const Name:String):TResType;
var ext:String;
begin
 ext:=UpperCase(ExtractExt(Name));
 if ext='.PCX' then Result:='MTXT'
 else if ext='.ITM' then Result:='METI'
 else if ext='.ATX' then Result:='FXTA'
 else if ext='.NWX' then Result:='FXAW'
 else if ext='.WAV' then Result:='DVAW'
 else if ext='.PHY' then Result:='SYHP'
 else if ext='.RCS' then Result:='BPCR'
 else if ext='.MSC' then Result:='BCSM'
 else if ext='.LAF' then Result:='TNFN'
 else if ext='.LVT' then Result:='FTVL'
 else if ext='.OBT' then Result:='FTBO'
 else if ext='.INF' then Result:='FFNI'
 else Result:=#0#0#0#0;
end;

Procedure TLABCreator.AddFile(F:TFile);
begin
 with TLABEntry(pes[centry]^) do
 begin
   offset:=cf.Fpos;
   size:=f.Fsize;
 end;
 CopyFileData(f,cf,f.Fsize);
 inc(centry);
end;

Function TLABCreator.ValidateName(name:String):String;
begin
 Result:=name;
end;

Destructor TLABCreator.Destroy;
var i:integer;
begin
 cf.Fseek(0);
 cf.FWrite(lh,sizeof(lh));
 for i:=0 to pes.count-1 do
 begin
  cf.FWrite(TLABEntry(pes[i]^),sizeof(TLABEntry));
  Dispose(pes[i]);
 end;
Inherited Destroy;
end;


{WAD procedures}

Type

TWEInfo=TFileInfo;

c8=array[0..7] of char;
wad_entry=packed record
 pentry:longint;
 length:longint;
 name:c8;
end;

wad_thing=record
 x,y:integer;
 angle:integer;
 thing:integer;
 attrib:integer;
end;

{The last two bytes of a THING control a few attributes, according to which bits are set:
bit 0   the THING is present at skill 1 and 2
bit 1   the THING is present at skill 3 (hurt me plenty)
bit 2   the THING is present at skill 4 and 5 (ultra-violence, nightmare)
bit 3   indicates a deaf guard.
bit 4   means the THING only appears in multiplayer mode.
bits 5-15 have no effect.}


wad_line=record
 fromvert,
 tovert,
 attr,
 linetype,
 tag,
 rightdef,
 leftdef:integer;
end;

{The third field of each linedef is an integer which controls that line's attributes with bits, as follows:

bit #  condition if it is set (=1)

bit 0   Impassable. Players and monsters cannot cross this line, and
          projectiles explode or end if they hit this line. Note, however,
          that if there is no sector on the other side, things can't go
          through this line anyway.
bit 1   Monster-blocker. Monsters cannot cross this line.
bit 2   Two-sided. If this bit is set, then the linedef's two sidedefs can
          have "-" as a texture, which means "transparent". If this bit is not
          set, the sidedefs can't be transparent: if "-" is viewed, it will
          result in the hall of mirrors effect. However, the linedef CAN have
          two non-transparent sidedefs, even if this bit is not set, if it is
          between two sectors.
        Another side effect of this bit is that if it is set, then
          projectiles and gunfire (pistol, etc.) can go through it if there
          is a sector on the other side, even if bit 0 is set.
        Also, monsters see through these lines, regardless of the line's
          other attribute settings and its textures ("-" or not doesn't matter).
bit 3   Upper texture is "unpegged". This is usually done at windows.
          "Pegged" textures move up and down when the neighbor sector moves
          up or down. For example, if a ceiling comes down, then a pegged
          texture on its side will move down with it so that it looks right.
          If the side isn't pegged, it just sits there, the new material is
          spontaneously created. The best way to get an idea of this is to
          change a linedef on an elevator or door, which are always pegged,
          and observe the result.
bit 4   Lower texture is unpegged.
bit 5   Secret. The automap will draw this line like a normal solid line that
          doesn't have anything on the other side, thus protecting the secret
          until it is opened. This bit is NOT what determines the SECRET
          ratio at the end of a level, that is done by special sectors (#9).
bit 6   Blocks sound. Sound cannot cross a line that has this bit set.
          Sound normally travels from sector to sector, so long as the floor
          and ceiling heights allow it (e.g. sound wouldn't go from a sector
          with 0/64 floor/ceiling height to one with 72/128, but sound WOULD
          go from a sector with 0/64 to one with 56/128).
bit 7   Not on map. The line is not shown on the regular automap, not even if
          the computer all-map power up is gained.
bit 8   Already on map. When the level is begun, this line is already on the
          automap, even though it hasn't been seen (in the display) yet.}

{Dec/Hex   ACT   EFFECT

-1 ffff   ? ?   None? Only once in whole game, on e2m7, (960,768)-(928,768)
0    00   - -   nothing
1    01   S R   DOOR. Sector on the other side of this line has its
                ceiling rise to 8 below the first neighbor ceiling on the
                way up, then comes back down after 6 seconds.
2    02   W 1   Open door (stays open)
3    03   W 1   Close door
5    05   W 1   Floor rises to match highest neighbor's floor height.
7    07   S 1   Staircase rises up from floor in appropriate sectors.
8    08   W 1   Stairs
9    09   S 1   Floor lowers; neighbor sector's floor rises and changes
                TEXTURE and sector type to match surrounding neighbor.
10   0a   W 1   Floor lowers for 3 seconds, then rises
11   0b   S -   End level. Go to next level.
13   0d   W 1   Brightness goes to 255
14   0e   S 1   Floor rises to 64 above neighbor sector's floor
16   10   W 1   Close door for 30 seconds, then opens.
18   12   S 1   Floor rises to equal first neighbor floor
19   13   W 1   Floor lowers to equal neighboring sector's floor
20   14   S 1   Floor rises, texture and sector type change also.
21   15   S 1   Floor lowers to equal neighbor for 3 seconds, then rises back
                up to stop 8 below neighbor's ceiling height
22   16   W 1   Floor rises, texture and sector type change also
23   17   S 1   Floor lowers to match lowest neighbor sector
26   1a   S R   DOOR. Need blue key to open. Closes after 6 seconds.
27   1b   S R   DOOR. Yellow key.
28   1c   S R   DOOR. Red key.
29   1d   S 1   Open door, closes after 6 seconds
30   1e   W 1   Floor rises to 128 above neighbor's floor
31   1f   S 1   DOOR. Stays open.
32   20   S 1   DOOR. Blue key. Stays open.
33   21   S 1   DOOR. Yellow key. Stays open.
34   22   S 1   DOOR. Red key. Stays open.
35   23   W 1   Brightness goes to 0.
36   24   W 1   Floor lowers to 8 above next lowest neighbor
37   25   W 1   Floor lowers, change floor texture and sector type
38   26   W 1   Floor lowers to match neighbor
39   27   W 1   Teleport to sector. Only ONE sector can have the same tag #
40   28   W 1   Ceiling rises to match neighbor ceiling
41   29   S 1   Ceiling lowers to floor
42   2a   S R   Closes door
44   2c   W 1   Ceiling lowers to 8 above floor
46   2e   G 1   Opens door (stays open)
48   30   - -   Animated, horizontally scrolling wall.
51   33   S -   End level. Go to secret level 9.
52   34   W -   End level. Go to next level
56   38   W 1   Crushing floor rises to 8 below neighbor ceiling
58   3a   W 1   Floor rises 32
59   3b   W 1   Floor rises 8, texture and sector type change also
61   3d   S R   Opens door
62   3e   S R   Floor lowers for 3 seconds, then rises
63   3f   S R   Open door, closes after 6 seconds
70   46   S R   Sector floor drops quickly to 8 above neighbor
73   49   W R   Start crushing ceiling, slow crush but fast damage
74   4a   W R   Stops crushing ceiling
75   4b   W R   Close door
76   4c   W R   Close door for 30 seconds
77   4d   W R   Start crushing ceiling, fast crush but slow damage
80   50   W R   Brightness to maximum neighbor light level
82   52   W R   Floor lowers to equal neighbor
86   56   W R   Open door (stays open)
87   57   W R   Start moving floor (up/down every 5 seconds)
88   58   W R   Floor lowers quickly for 3 seconds, then rises
89   59   W R   Stops the up/down syndrome started by #87
90   5a   W R   Open door, closes after 6 seconds
91   5b   W R   Floor rises to 8 below neighbor ceiling
97   61   W R   Teleport to sector. Only ONE sector can have the same tag #
98   62   W R   Floor lowers to 8 above neighbor
102  66   S 1   Floor lowers to equal neighbor

103  67   S 1   Opens door (stays open)
104  68   W 1   Light drops to lowest light level amongst neighbor sectors}
{     * FLAG 1
     +-----------------------------------+
     |     1 ADJOINING MID TX            | the MID TX is NOT removed
     |     2 ILLUMINATED SIGN            |
     |     4 FLIP TEXTURE HORIZONTALLY   |
     |     8 ELEV CAN CHANGE WALL LIGHT  |
     |    16 WALL TX ANCHORED            |
     |    32 WALL MORPH WITH SECTOR      |
     |    64 ALLOW SCROLL TOP  TX        |
     |   128 ALLOW SCROLL MID  TX        |
     |   256 ALLOW SCROLL BOT  TX        |
     |   512 ALLOW SCROLL SIGN TX        |
     |  1024 HIDE ON MAP                 |
     |  2048 SHOW ON MAP                 |
     |  4096 SIGN ANCHORED               |
     |  8192 WALL DAMAGES PLAYER         |
     | 16384 SHOW AS LEDGE ON MAP        |
     | 32768 SHOW AS DOOR  ON MAP        |
     +-----------------------------------+

     * FLAG 2 is unused.

     * FLAG 3
     +-----------------------------------+
     |     1 CAN ALWAYS WALK             |
     |     2 CANNOT WALK THROUGH WALL    |
     |     4 FENCE - KEEP ENNEMIES IN    |
     |     8 CANNOT FIRE THROUGH WALL    |
     +-----------------------------------+}


wad_side=record
 xoffs,
 yoffs:integer;
 uptext,
 lowtext,
 midtext:c8;
 sector:integer;
end;

wad_vertex=record
 x,y:integer;
end;

wad_sector=record
 fheight,
 cheight:integer;
 ftext,
 ctext:c8;
 light,
 sectype,
 tag:integer;
end;

{
0   00  is normal, no special characteristic.
1   01  light blinks off at random times.
2   02  light blinks on every 0.5 second
3   03  light blinks on every 1.0 second
4   04  -10/20% health AND light blinks on every 0.5 second
5   05  -5/10% health
7   07  -2/5% health, this is the usual NUKAGE acid-floor.
8   08  light oscillates
9   09  SECRET: player must move into this sector to get credit for finding
        this secret. Counts toward the ratio at the end of the level.
10  0a  ?, ceiling comes down 30 seconds after level is begun
11  0b  -10/20% health. When player's HEALTH <= 10%, then the level ends. If
        it is a level 8, then the episode ends.
12  0c  light blinks on every 1.0 second
13  0d  light blinks on every 0.5 second
14  0e  ??? Seems to do nothing
16  10  -10/20% health}

{    +-----------------------------------+
     |      1 EXTERIOR - NO CEIL.  (SKY) |
     |      2 DOOR                       | instant door
     |      4 SHOT REFLEXION / MAG.SEAL  | walls reflect weapon shots
     |      8 EXTERIOR ADJOIN            | for adjacent skys with != alt.
     |     16 ICE FLOOR        (SKATING) |
     |     32 SNOW FLOOR                 |
     |     64 EXPLODING WALL/DOOR        | instant exploding door
     |    128 EXTERIOR - NO FLOOR  (PIT) |
     |    256 EXTERIOR FLOOR ADJOIN      | for adjacent pits with != alt.
     |    512 CRUSHING SECTOR            |
     |   1024 NO WALL DRAW / "HORIZON"   | horizon moves with eye level
     |   2048 LOW  DAMAGE                |
     |   4096 HIGH DAMAGE   (both = GAS) | both can be combined for GAS
     |   8192 NO SMART OBJECT REACTION   |
     |  16384 SMART OBJECT REACTION      |
     |  32768 SUBSECTOR                  | player won't follow moving SC
     |  65536 SAFE SECTOR                |
     | 131072 RENDERED     (do not use!) |
     | 262144 PLAYER       (do not use!) |
     | 524288 SECRET SECTOR              | increments the %secret when entered
     +-----------------------------------+}


Procedure TWADDirectory.Refresh;
var f:TFile;
    we:Wad_entry;
    Fi:TWEInfo;
    i:integer;
    s:string;
    ename:array[0..8] of char;
begin
 ClearIndex;
 f:=OpenFileRead(name,0);
Try
 F.FRead(wh,sizeof(wh));
 if (wh.magic<>'IWAD') and (wh.magic<>'PWAD') then raise Exception.Create(Name+' is not a WAD file');
 F.FSeek(wh.pdirectory);
 for i:=0 to wh.nentries-1 do
 begin
  F.FRead(we,sizeof(we));
  fi:=TWEInfo.Create;
  fi.offs:=we.Pentry;
  fi.size:=we.Length;
  Files.AddObject(StrLCopy(ename,we.name,8),fi);
 end;
Finally
 F.FClose;
end;
end;

{GOB2}
Procedure TGOB2Directory.Refresh;
var Fi:TFInfo;
    i:integer;
    ge:TGob2Entry;
    s:string;
    f:TFile;
    dir:string;
begin
 ClearIndex;
 f:=OpenFileRead(name,0);
Try
 F.FRead(gh,sizeof(gh));
 if gh.magic<>'GOB ' then raise Exception.Create(Name+' is not a GOB 2.0 file');
 for i:=0 to gh.NEntries-1 do
 begin
  F.FRead(ge,sizeof(ge));
  fi:=TFInfo.Create;
  fi.offs:=ge.pos;
  fi.size:=ge.size;
  Files.AddObject(ge.name,fi);
  Dir:=ExtractFilePath(ge.name);
  if dir<>'' then
  begin
   If Dir[Length(Dir)]='\' then SetLength(Dir,Length(Dir)-1);
   if Dirs.IndexOf(Dir)=-1 then Dirs.Add(Dir);
  end;
 end;
Finally
 F.FClose;
end;
end;

Function TGOB2Directory.GetContainerCreator(name:String):TContainerCreator;
begin
 Raise Exception.Create('Not yet implemented');
end;

Function TGOB2Directory.GetColWidth(n:Integer):Integer;
begin
 case n of
  0: Result:=31;
 else Result:= Inherited GetColWidth(n);
 end;
end;


Procedure TGOB2Creator.PrepareHeader(newfiles:TStringList);
var i:Integer;
    pe:^TGOB2Entry;
    aName:STring;
begin
 pes:=TList.Create;
 cf.FSeek(sizeof(TGOB2Header)+newfiles.count*Sizeof(TGOB2Entry));
 for i:=0 to newfiles.count-1 do
 begin
  New(pe);
  aName:=Newfiles[i];
  FillChar(pe^.name,sizeof(pe^.name),0);
  StrPCopy(Pe^.Name,ValidateName(aName));
  pes.Add(pe);
 end;
 centry:=0;
end;

Procedure TGOB2Creator.AddFile(F:TFile);
begin
 with Tgob2Entry(pes[centry]^) do
 begin
  pos:=cf.Fpos;
  size:=f.Fsize;
 end;
 CopyFileData(F,CF,f.Fsize);
 inc(Centry);
end;

Function TGOB2Creator.ValidateName(name:String):String;
var n,ext:String;
begin
 Result:=LowerCase(name);
{ Ext:=ExtractExt(name);
 n:=Copy(Name,1,length(Name)-length(ext));
 if length(Ext)>4 then SetLength(Ext,4);
 if length(N)>8 then SetLength(N,8);
 Result:=N+ext;}
end;

Destructor TGOB2Creator.Destroy;
var i:integer;
    gh:TGob2Header;
    n:Longint;
begin
if pes<>nil then
begin
 gh.magic:='GOB ';
 gh.Long1:=$14;
 gh.long2:=$c;
 gh.NEntries:=pes.count;

 cf.Fseek(0); cf.FWrite(gh,sizeof(gh));

 for i:=0 to pes.count-1 do
 begin
  cf.FWrite(TGOB2Entry(pes[i]^),sizeof(TGOB2Entry));
  Dispose(pes[i]);
 end;
 pes.Free;
end;
Inherited Destroy;
end;


end.
