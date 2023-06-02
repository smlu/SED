unit graph_files;

interface
uses Images, Graphics, Files, FileOperations, SysUtils, Misc_Utils;
Type

TMatHeader = record
tag:array[0..3] of char;     {'MAT ' - notice space after MAT}
ver:Longint;             {Apparently - version = 0x32 ('2')}
mat_Type:Longint;            {0 = colors(TColorHeader) , 1= ?, 2= texture(TTextureHeader)}
NumOfTextures:Longint;   {number of textures or colors}
NumOfTextures1: Longint; { In color MATs, it's 0, in TX ones, it's equal to numOfTextures }
pad1:Longint;                 { = 0 }
numbits:LongInt;                 { = 8 }
rBits,
gBits,
bBits:longint;
pads:array[0..8]of longint;  {unknown. Some pad?}
end;

TColorHeader = record
textype:longint;         {0 = color, 8= texture}
colornum:longint;        {Color index from the CMP palette}
pads:array[0..3]of Longint;   {each = 0x3F800000 (check cmp header )}
end;

TTextureHeader = record
textype:longint;         {0 = color, 8= texture}
colornum:longint;        {unknown use}
pads:array[0..3]of Longint;   {each longint = 0x3F800000 (check cmp header )}
pads1:array[0..1]of Longint;   {unknown}
pad2:Longint;                 {=0xBFF78482}
CurrentTXNum:Longint     {number of corresponding texture, beginning with 0, ranging to NumOfTextures-1}
end;

TTextureData = record
SizeX:Longint;             {horizontal size of first MipMap, must be divisable by 2}
SizeY:Longint;             {Vertical size of first MipMap ,must be divisable by 2}
Pad:array[0..2]of LongInt; {padding = 0 }
NumMipMaps:LongInt;        {Number of mipmaps in texture largest one first.}
end;

TCMPHeader=record
 sig:array[0..3] of char; {'CMP '}
 twenty:longint;
 HasTransparency:Longint;
 stuff:array[1..52] of byte;
end;

TCMPPal=array[0..255] of record r,g,b:byte; end;
TCMPTable=array[0..255] of byte;


	TPCXHeader=packed record
	 manuf,
	 hard,
	 encod,
	 bitperpixel:byte;
	 x1,y1,x2,y2,
	 hres,vres:word;
	 palette:array[0..15] of record 	red,green,blue:byte; end;
	 vmode,
	 nplanes:byte;
	 byteperline,
	 palinfo,
	 shres,svres:word;
	 extra:array[0..53] of byte;
	end;

Type
TPCX=class(TImageSource)
 f:TFile;
 ph:TPCXHeader;
 lbuf:array[0..2047] of byte;
 bpos,bsize:Integer;
 Constructor Create(aF:TFile);
 Destructor Destroy;Override;
 procedure GetLine(var buf);override;
end;

TMAT=class(TImageSource)
 f:TFile;
 mh:TMATHeader;
 th:TTextureHeader;
 ch:TCOlorHeader;
 td:TTextureData;
 iscolor:boolean;
 isAnimated:boolean;
 nCurCell:word;
 colornum:byte;
 txoffs:longint;
 Constructor Create(aF:TFile;nCell:integer);
 Destructor Destroy;Override;
 procedure GetLine(var buf);override;
 Procedure SetPal(cmppal:TCMPPal);
 Procedure LoadBits(var buf);
end;


Function LoadCMPPal(const cmpname:string; var pal:TCMPPal):boolean; {loads palette from CMP}
Function ApplyCMP(const cmpname:string; var pal:TCMPPal):boolean; {applies CMP to the palette}
Function LoadCMPTable(const cmpname:string;var cmp:TCMPTable):boolean;
Procedure ApplyCMPTable(var pal:TCMPPal;const cmp:TCMPTable);

var
 defCmppal:TCMPPal=(
 (r:0;g:0;b:0),
 (r:0;g:255;b:0),
 (r:0;g:203;b:0),
 (r:0;g:155;b:0),
 (r:0;g:107;b:0),
 (r:0;g:59;b:0),
 (r:255;g:0;b:0),
 (r:203;g:0;b:0),
 (r:155;g:0;b:0),
 (r:107;g:0;b:0),
 (r:59;g:0;b:0),
 (r:247;g:255;b:0),
 (r:215;g:163;b:0),
 (r:175;g:87;b:0),
 (r:135;g:31;b:0),
 (r:95;g:0;b:0),
 (r:255;g:255;b:255),
 (r:223;g:231;b:255),
 (r:195;g:215;b:255),
 (r:163;g:195;b:255),
 (r:135;g:175;b:255),
 (r:255;g:171;b:0),
 (r:255;g:159;b:0),
 (r:255;g:147;b:0),
 (r:255;g:131;b:0),
 (r:255;g:111;b:0),
 (r:255;g:91;b:0),
 (r:255;g:71;b:0),
 (r:255;g:51;b:0),
 (r:255;g:35;b:0),
 (r:255;g:15;b:0),
 (r:0;g:0;b:255),
 (r:253;g:253;b:253),
 (r:247;g:247;b:247),
 (r:239;g:239;b:239),
 (r:227;g:227;b:227),
 (r:219;g:219;b:219),
 (r:211;g:211;b:211),
 (r:203;g:203;b:203),
 (r:195;g:195;b:195),
 (r:187;g:187;b:187),
 (r:179;g:179;b:179),
 (r:171;g:171;b:171),
 (r:163;g:163;b:163),
 (r:155;g:155;b:155),
 (r:147;g:147;b:147),
 (r:139;g:139;b:139),
 (r:131;g:131;b:131),
 (r:123;g:123;b:123),
 (r:115;g:115;b:115),
 (r:107;g:107;b:107),
 (r:99;g:99;b:99),
 (r:87;g:87;b:87),
 (r:79;g:79;b:79),
 (r:71;g:71;b:71),
 (r:63;g:63;b:63),
 (r:55;g:55;b:55),
 (r:47;g:47;b:47),
 (r:39;g:39;b:39),
 (r:31;g:31;b:31),
 (r:23;g:23;b:23),
 (r:15;g:15;b:15),
 (r:7;g:7;b:7),
 (r:0;g:0;b:0),
 (r:191;g:199;b:223),
 (r:183;g:191;b:215),
 (r:179;g:183;b:207),
 (r:171;g:179;b:203),
 (r:163;g:171;b:195),
 (r:159;g:163;b:187),
 (r:151;g:159;b:183),
 (r:147;g:151;b:175),
 (r:139;g:143;b:167),
 (r:135;g:139;b:163),
 (r:127;g:131;b:155),
 (r:119;g:123;b:147),
 (r:115;g:119;b:139),
 (r:107;g:111;b:135),
 (r:103;g:107;b:127),
 (r:95;g:99;b:119),
 (r:91;g:95;b:115),
 (r:87;g:87;b:107),
 (r:79;g:83;b:99),
 (r:75;g:75;b:95),
 (r:67;g:71;b:87),
 (r:63;g:63;b:79),
 (r:55;g:59;b:75),
 (r:51;g:51;b:67),
 (r:47;g:47;b:59),
 (r:39;g:43;b:55),
 (r:35;g:35;b:47),
 (r:31;g:31;b:39),
 (r:23;g:27;b:35),
 (r:19;g:19;b:27),
 (r:15;g:15;b:19),
 (r:11;g:11;b:15),
 (r:255;g:207;b:179),
 (r:231;g:175;b:143),
 (r:207;g:143;b:111),
 (r:183;g:119;b:87),
 (r:159;g:91;b:63),
 (r:135;g:71;b:43),
 (r:111;g:51;b:27),
 (r:87;g:35;b:15),
 (r:255;g:255;b:0),
 (r:227;g:195;b:0),
 (r:199;g:143;b:0),
 (r:171;g:99;b:0),
 (r:147;g:63;b:0),
 (r:119;g:31;b:0),
 (r:91;g:11;b:0),
 (r:67;g:0;b:0),
 (r:223;g:255;b:167),
 (r:207;g:239;b:135),
 (r:191;g:223;b:103),
 (r:179;g:207;b:75),
 (r:167;g:191;b:51),
 (r:159;g:175;b:31),
 (r:151;g:159;b:11),
 (r:143;g:147;b:0),
 (r:199;g:99;b:31),
 (r:183;g:87;b:23),
 (r:171;g:75;b:19),
 (r:155;g:63;b:11),
 (r:143;g:55;b:7),
 (r:127;g:47;b:7),
 (r:115;g:39;b:0),
 (r:103;g:31;b:0),
 (r:251;g:0;b:0),
 (r:227;g:0;b:0),
 (r:199;g:0;b:0),
 (r:171;g:0;b:0),
 (r:143;g:0;b:0),
 (r:115;g:0;b:0),
 (r:87;g:0;b:0),
 (r:57;g:0;b:0),
 (r:127;g:163;b:199),
 (r:95;g:127;b:171),
 (r:67;g:95;b:147),
 (r:43;g:67;b:123),
 (r:23;g:39;b:95),
 (r:11;g:19;b:71),
 (r:0;g:7;b:47),
 (r:0;g:0;b:23),
 (r:195;g:115;b:71),
 (r:183;g:107;b:63),
 (r:175;g:99;b:59),
 (r:163;g:91;b:51),
 (r:155;g:87;b:47),
 (r:147;g:79;b:43),
 (r:135;g:71;b:35),
 (r:127;g:67;b:31),
 (r:115;g:59;b:27),
 (r:107;g:55;b:23),
 (r:99;g:47;b:19),
 (r:87;g:43;b:15),
 (r:79;g:39;b:15),
 (r:67;g:31;b:11),
 (r:59;g:27;b:7),
 (r:51;g:23;b:7),
 (r:255;g:231;b:179),
 (r:239;g:211;b:155),
 (r:223;g:195;b:135),
 (r:211;g:179;b:119),
 (r:195;g:163;b:99),
 (r:183;g:147;b:83),
 (r:167;g:135;b:71),
 (r:151;g:119;b:55),
 (r:139;g:103;b:43),
 (r:123;g:91;b:31),
 (r:111;g:79;b:23),
 (r:95;g:67;b:15),
 (r:79;g:55;b:11),
 (r:67;g:43;b:7),
 (r:51;g:31;b:0),
 (r:39;g:23;b:0),
 (r:131;g:231;b:103),
 (r:115;g:207;b:83),
 (r:99;g:183;b:67),
 (r:83;g:159;b:55),
 (r:71;g:139;b:43),
 (r:59;g:115;b:31),
 (r:47;g:91;b:23),
 (r:35;g:71;b:15),
 (r:255;g:167;b:255),
 (r:223;g:127;b:231),
 (r:195;g:95;b:207),
 (r:163;g:67;b:183),
 (r:135;g:43;b:159),
 (r:107;g:23;b:135),
 (r:79;g:7;b:111),
 (r:55;g:0;b:91),
 (r:255;g:0;b:255),
 (r:255;g:0;b:255),
 (r:255;g:0;b:255),
 (r:255;g:0;b:255),
 (r:255;g:0;b:255),
 (r:255;g:0;b:255),
 (r:255;g:0;b:255),
 (r:255;g:0;b:255),
 (r:255;g:0;b:255),
 (r:255;g:0;b:255),
 (r:255;g:0;b:255),
 (r:255;g:0;b:255),
 (r:255;g:0;b:255),
 (r:255;g:0;b:255),
 (r:255;g:0;b:255),
 (r:255;g:0;b:255),
 (r:255;g:0;b:255),
 (r:255;g:0;b:255),
 (r:255;g:0;b:255),
 (r:255;g:0;b:255),
 (r:255;g:0;b:255),
 (r:255;g:0;b:255),
 (r:255;g:0;b:255),
 (r:255;g:0;b:255),
 (r:255;g:0;b:255),
 (r:255;g:0;b:255),
 (r:255;g:0;b:255),
 (r:255;g:0;b:255),
 (r:255;g:0;b:255),
 (r:255;g:0;b:255),
 (r:255;g:0;b:255),
 (r:255;g:0;b:255),
 (r:255;g:0;b:255),
 (r:255;g:0;b:255),
 (r:255;g:0;b:255),
 (r:255;g:0;b:255),
 (r:255;g:0;b:255),
 (r:255;g:0;b:255),
 (r:255;g:0;b:255),
 (r:255;g:0;b:255),
 (r:255;g:0;b:255),
 (r:255;g:0;b:255),
 (r:255;g:0;b:255),
 (r:255;g:0;b:255),
 (r:255;g:0;b:255),
 (r:255;g:0;b:255),
 (r:255;g:0;b:255),
 (r:255;g:0;b:255),
 (r:255;g:0;b:255),
 (r:255;g:0;b:255),
 (r:255;g:0;b:255),
 (r:255;g:0;b:255),
 (r:255;g:0;b:255),
 (r:255;g:0;b:255),
 (r:255;g:0;b:255),
 (r:255;g:0;b:255),
 (r:255;g:0;b:255),
 (r:255;g:0;b:255),
 (r:255;g:0;b:255),
 (r:255;g:0;b:255),
 (r:255;g:0;b:255),
 (r:255;g:0;b:255),
 (r:255;g:0;b:255),
 (r:255;g:255;b:255));

implementation

type
TPCXPal=array[0..255] of record r,g,b:byte; end;

Function ApplyCMP(const cmpname:string; var pal:TCMPPal):boolean; {applies CMP to the palette}
var ct:TCMPTable;
begin
 result:=false;
 if not LoadCMPTable(cmpname,ct) then exit;
 ApplyCMPTable(pal,ct);
 result:=true;
end;

Function LoadCMPTable(const cmpname:string;var cmp:TCMPTable):boolean;
var
    f:TFile;
begin
result:=true;
try
 f:=OpenGameFile(cmpname);
 f.Fseek(Sizeof(tCMPHeader)+sizeof(TCMPPal)+63*sizeof(TCMPTable));
 f.Fread(cmp,sizeof(cmp));
 F.Fclose;
except
 on Exception do result:=false;
end;
end;



Procedure ApplyCMPTable(var pal:TCMPPal;const cmp:TCMPTable);
var i:integer;
    pnew:TCMPPal;
begin
 for i:=0 to 255 do pnew[i]:=pal[cmp[i]];
 pal:=pnew;
end;

Function LoadCMPPal(const cmpname:string; var pal:TCMPPal):boolean; {loads palette from CMP}
var f:TFile;
begin
Result:=true;
try
 f:=OpenGameFile(cmpname);
 f.Fseek(Sizeof(tCMPHeader));
 f.Fread(pal,sizeof(pal));
 f.Fclose;
except
 On Exception do begin pal:=defCmpPal; result:=false; end;
end;
end;


Procedure TMAT.SetPal(cmppal:TCMPPal);
var i:integer;
begin
 for i:=0 to 255 do
 With CmpPal[i],Pal[i] do
 begin
  rgbRed:=r;
  rgbGreen:=g;
  rgbBlue:=b;
 end;
end;

Destructor TMAT.Destroy;
begin
 if f<>nil then f.Fclose;
 Inherited Destroy;
end;

Constructor TMAT.Create(aF:TFile;nCell:integer);
var
   i,j:Integer;
   msize,w,h:integer;
begin
 Inherited Create;
 f:=Af;

 f.fread(mh,sizeof(mh));
 if mh.tag<>'MAT ' then Raise Exception.Create('Not a MAT file');


 if mh.mat_type=0 then
 begin
  iscolor:=true;
  f.Fread(ch,sizeof(ch));
  FInfo.width:=64;
  FInfo.height:=64;
  FInfo.StoredAs:=ByLines;
  colornum:=ch.colornum;
 end
 else
 begin
  iscolor:=false;
  f.FSeek(sizeof(mh)+mh.NumOfTextures*sizeof(th));

  if nCell>=mh.NumOfTextures then nCurCell:=mh.NumOfTextures-1
  else nCurCell:=nCell;

  for i:=0 to nCurCell do
  begin
   f.Fread(td,sizeof(td));
   FInfo.width:=td.sizeX;
   FInfo.height:=td.SizeY;
   w:=td.sizeX;
   h:=td.SizeY;
   msize:=0;
   for j:=0 to td.NumMipMaps-1 do
   begin
    msize:=msize+w*h;
    if mh.numbits=16 then msize:=msize*2;
    w:=w div 2;
    h:=h div 2;
   end;

   txoffs:=f.fpos;
   f.FSeek(txoffs+msize);
  end;

  F.Fseek(txoffs);

  if mh.numBits=16 then  FInfo.StoredAs:=ByLines16 else
  FInfo.StoredAs:=ByLines;
 end;

 isAnimated:=(mh.NumOfTextures>1) and (not IsColor);
end;

Procedure TMAT.LoadBits(var buf);
begin
 if IsColor then
 begin
  FillChar(buf,FInfo.width*FInfo.Height,ColorNum);
  exit;
 end;
 if mh.numBits=16 then F.Fread(buf,FInfo.width*FInfo.Height*2)
 else F.Fread(buf,FInfo.width*FInfo.Height);
end;

procedure TMAT.GetLine(var buf);
begin
 if IsColor then
 begin
  FillChar(buf,FInfo.width,ColorNum);
  exit;
 end;
 if mh.numBits=16 then F.Fread(buf,FInfo.width*2)
 else F.Fread(buf,FInfo.width);
end;



Destructor TPCX.Destroy;
begin
 if f<>nil then f.Fclose;
 Inherited Destroy;
end;

Constructor TPCX.Create(aF:TFile);
var
   Pxpal:TPCXPal;
   ptype:byte;
   i:Integer;
begin
 Inherited Create;
 f:=Af;

 f.fread(ph,sizeof(ph));
 if (ph.nPlanes<>1) or (ph.BitPerPixel<>8) then
 begin
  Raise Exception.Create('Not a 256 color PCX');
 end;
 FInfo.width:=ph.x2-ph.x1+1;
 FInfo.height:=ph.y2-ph.y1+1;
 FInfo.StoredAs:=ByLines;

 F.Fseek(F.Fsize-sizeof(pxpal)-sizeof(ptype));
 F.fread(ptype,sizeof(ptype));
 F.fread(pxpal,sizeof(pxpal));

 case ptype of
  10: for i:=0 to 255 do
  with pal[i] do
   begin
    rgbRed:=pxpal[i].r*4;
    rgbGreen:=pxpal[i].g*4;
    rgbBlue:=pxpal[i].b*4;
    rgbReserved:=0;
   end;
  12: for i:=0 to 255 do
  with pal[i] do
   begin
    rgbRed:=pxpal[i].r;
    rgbGreen:=pxpal[i].g;
    rgbBlue:=pxpal[i].b;
    rgbReserved:=0;
   end;
  else Raise Exception.Create('Cannot locate palette');
 end;
 f.Fseek(sizeof(ph));
 bpos:=0;
 bsize:=0;
end;

procedure TPCX.GetLine(var buf);
function GetNextByte:Byte;
begin
 if Bpos>=bsize then
 begin
  bsize:=F.FSize-F.Fpos;
  if bsize>sizeof(lbuf) then bsize:=sizeof(lbuf);
  F.Fread(Lbuf,bsize);
  bpos:=0;
 end;
 Result:=lbuf[bpos];inc(bpos);
end;

var
  cw,bw:Integer;
  c,b1:byte;
  pbuf:^byte;
begin
 bw:=ph.byteperline;
 pbuf:=@buf; cw:=0;
 while cw<bw do
 begin
  c:=getnextbyte;
  if (c and $C0)<>$c0 then begin pbuf^:=c; inc(Pbuf); inc(cw); end
  else
  begin
   b1:=getnextbyte;
   c:=c and 63;
   fillchar(Pbuf^,c,b1);
   inc(pBuf,c); inc(cw,c);
  end;
 end;
   if cw<>bw then PanMessage(mt_Warning,'Warning, bad data in the PCX file');
end;

(*
Function loadPCX(var f:file; var b:tbitmap):integer;
type line=array[0..0] of byte;
var
    h:pcxheader;
    pal:tpal;
    pos:longint;
    ptype:byte; {Palette type 10 - 0..63, 12 - 0..255}
    i:integer; p:pointer;
    bp,cw,bw:integer;
    b1,c:byte;

Function getnextbyte:byte;
var c:byte;
begin
if bp<sizeof(buf) then begin getnextbyte:=buf[bp]; inc(bp); end
    else begin fread(f,buf,sizeof(buf)); bp:=1; getnextbyte:=buf[0]; end;
 if inoutres=0 then if errorcode<>0 then;
end;

begin
 loadpcx:=-1;
 fread(f,h,sizeof(h));
 if (h.manuf<>10) and (h.hard<>5) then begin Fileerror('Only PCX version 5 is supported',f); exit; end;
 if (h.bitperpixel<>8) or (h.nplanes<>1) then begin Fileerror('Not a 256 color PCX',f); exit; end;
 seek(f,filesize(f)-sizeof(pal)-sizeof(ptype));
 fread(f,ptype,sizeof(ptype));
 fread(f,pal,sizeof(pal));
 case ptype of
  10: for i:=0 to 255 do
  with b.pal[i] do
   begin
    red:=pal[i].red*4;
    green:=pal[i].green*4;
    blue:=pal[i].blue*4;
   end;
  12: b.pal:=pal;
  else begin Fileerror('Cannot locate palette',f); exit; end;
 end;

 b.w:=h.x2-h.x1+1;
 b.h:=h.y2-h.y1+1;
 if enoughmem(b.w,b.h,f)=-1 then exit;
 b.bits:=heapptr;
 bw:=h.byteperline;
 seek(f,sizeof(h));
 bp:=sizeof(buf);
 for i:=b.h-1 downto 0 do
  begin
   cw:=0;
   while cw<bw do
   begin
    c:=getnextbyte;
    if (c and $C0)<>$c0 then begin cbuf[cw]:=c; inc(cw); end
    else
    begin
     b1:=getnextbyte;
     c:=c and 63;
     fillchar(cbuf[cw],c,b1);
     inc(cw,c);
    end;
   end;
   move(cbuf,ip(b.bits,b.w*i)^,b.w);
   if cw<>bw then Writeln('Warning, bad data in file ',getname(f));
   {write(cw-bw,'  '#13); seek(f,filepos(f)+(bw-b.w));}
  end;
 if errorcode=0 then loadpcx:=0 else Fileerror('Read fault',f);
end;}

function pcx_compress(ib,ob:pchar;size:word):word;
label _store;
var pi,po:pchar;
    c:char;
    left:word;
    rep:word;
begin
 Left:=size;pi:=ib;po:=ob;
While left>0 do
begin
 c:=pi^;inc(pi); dec(left);
 if (left=0) or (c<>pi^) then
 begin
  if (ord(c) and $C0)<>$C0 then begin po^:=c; inc(po); end
  else begin po^:=#$C1; inc(po); po^:=c; inc(po); end;
 end
 else
 begin
  rep:=2;dec(left);inc(pi);
  While (left>0) and (pi^=c) do begin inc(rep); dec(left); inc(pi); end;
  While rep>63 do begin po^:=#$FF; inc(po); po^:=c; inc(po); Dec(rep,63); end;
  if rep=0 then continue;
  if (rep=1) and ((ord(c) and $C0)<>$C0) then begin po^:=c; inc(po); end
  else begin po^:=chr($C0+rep); inc(po); po^:=c; inc(po); end;
 end;
end;
pcx_compress:=po-ob;
end;

{Function SavePCX(var f:file; var b:tbitmap):integer; {bottom-up bitmap}
type line=array[0..0] of byte;
var
    h:pcxheader;
    pos:longint;
    ptype:byte; {Palette type 10 - 0..63, 12 - 0..255}
    i:integer; p:pointer;
    bw:integer;

begin
 savepcx:=-1;
 h.manuf:=10;
 h.hard:=5;
 h.encod:=1;
 h.bitperpixel:=8;
 h.x1:=0; h.y1:=0;
 h.x2:=b.w-1; h.y2:=b.h-1;
 h.hres:=320;
 h.vres:=200;
 fillchar(h.palette,sizeof(h.palette),0);
 h.vmode:=$13;
 h.nplanes:=1;
 h.byteperline:=b.w;
 h.palinfo:=1;
 h.shres:=300; h.svres:=300;
 fillchar(h.extra,sizeof(h.extra),0);
 fwrite(f,h,sizeof(h));
 for i:=b.h-1 downto 0 do
 begin
  bw:=pcx_compress(ip(b.bits,b.w*i),@cbuf,b.w);
  fwrite(f,cbuf,bw);
 end;
 ptype:=12;
 fwrite(f,ptype,1);
 fwrite(f,b.pal,sizeof(b.pal));
 if errorcode=0 then savepcx:=0 else Fileerror('Write fault',f);
end;} *)


end.

end.
