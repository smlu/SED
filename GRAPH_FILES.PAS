unit graph_files;

interface

uses Images, Graphics, Files, FileOperations, SysUtils, Misc_Utils, Math;

type
  TMatHeader = record
    tag: array [0 .. 3] of AnsiChar; // 'MAT ' - notice space after MAT
    version: Longint; //Apparently - version = 0x32 ('2')
    mat_Type: Longint;  // 0 = colors(TColorHeader) , 1= ?, 2= texture(TTextureHeader)
    celCount: Longint;  // number of celluloid texture or color records }
    textureCount: Longint; // In color MATs, it's 0, in TX ones, it's equal to celCount
    ci: TColorInfo;
  end;

  TColorHeader = record
    textype: Longint; { 0 = color, 8= texture }
    colornum: Longint; { Color index from the CMP palette }
    pads: array [0 .. 3] of Longint; { each = 0x3F800000 (check cmp header ) }
  end;

  TTextureHeader = record
    textype: Longint; { 0 = color, 8= texture }
    colornum: Longint; { unknown use }
    pads: array [0 .. 3] of Longint;
    { each longint = 0x3F800000 (check cmp header ) }
    pads1: array [0 .. 1] of Longint; { unknown }
    pad2: Longint; { =0xBFF78482 }
    CurrentTXNum
      : Longint { number of corresponding texture, beginning with 0, ranging to NumOfTextures-1 }
    end;

  TTextureData = record
    SizeX: Longint;
    { horizontal size of first MipMap, must be divisable by 2 }
    SizeY: Longint; { Vertical size of first MipMap ,must be divisable by 2 }
    Pad: array [0 .. 2] of Longint; { padding = 0 }
    NumMipMaps: Longint; { Number of mipmaps in texture largest one first. }
  end;

  TCMPHeader = record
    sig: array [0 .. 3] of AnsiChar; { 'CMP ' }
    twenty: Longint;
    HasTransparency: Longint;
    stuff: array [1 .. 52] of byte;
  end;


TCMPTable = array [0 .. 255] of byte;
PTCMPTable = ^TCMPTable;

TPCXHeader = packed record
  manuf, hard, encod, bitperpixel: byte;
  x1, y1, x2, y2, hres, vres: word;
  palette: array [0 .. 15] of record
    red, green, blue: byte;
  end;
  vmode, nplanes: byte;
  byteperline, palinfo, shres, svres: word;
  extra: array [0 .. 53] of byte;
end;

Type
  TPCX = class(TImageSource)
    f: TFile;
    ph: TPCXHeader;
    lbuf: array [0 .. 2047] of byte;
    bpos, bsize: Integer;
    Constructor Create(aF: TFile);
    Destructor Destroy; Override;
    procedure ReadRow(var buf); override;
  end;

  TMAT = class(TImageSource)
    f: TFile;
    //mh: TMatHeader;
    celCount: Longint;
    th: TTextureHeader;
    ch: TColorHeader;
    td: TTextureData;
    iscolor: boolean;
    isAnimated: boolean;
    nCurCell: word;
    colornum: byte;
    txoffs: Longint;
    Constructor Create(aF: TFile; nCell: Integer);
    Destructor Destroy; Override;
    Function Stride: Uint32; // row length in bytes as bpp/8 * width
    Procedure ReadRow(var buf); override; // reads image row from file and writes it to buf
    Procedure ReadImage(pDestBuf: Pointer; dstci: TColorInfo; pitch: LongInt; const ppal:PTCMPPal; gamma: double = 1.0);overload; override;
    Procedure ReadImage(pDestBuf: Pointer; const dstci: TColorInfo; const pitch: LongInt; const gamma: double = 1.0); overload; override; // Reads RGB(A) texture from file, encodes to dstci and writes it to buffer
    Procedure SetPal(cmppal: TCMPPal);
    Procedure LoadBits(var buf);

  private
    Procedure RecodeTextureRow(srcRow: PByte; dstRow: PByte; dstci: TColorInfo; gamma : double);  // Decodes RGB(A) texture row to dstci and writes to dstRow.
  end;

Function EncodePixel(r,g,b,a : Uint32 ; dstci: TColorInfo; gamma: double = 1.0): UInt32; inline;
Function RecodePixel(pixel: Uint32; pixci: TColorInfo; dstci: TColorInfo; gamma: double = 1.0): Uint32; inline;

Function LoadCMPPal(const cmpname: string; var pal: TCMPPal): boolean;
{ loads palette from CMP }
  Function ApplyCMP(const cmpname: string; var pal: TCMPPal): boolean;
  { applies CMP to the palette }
    Function LoadCMPTable(const cmpname: string; var cmp: TCMPTable): boolean;
      Procedure ApplyCMPTable(var pal: TCMPPal; const cmp: TCMPTable);

      var
        defCmppal: TCMPPal = ((r: 0; g: 0; b: 0), (r: 0; g: 255; b: 0), (r: 0;
          g: 203; b: 0), (r: 0; g: 155; b: 0), (r: 0; g: 107; b: 0), (r: 0;
          g: 59; b: 0), (r: 255; g: 0; b: 0), (r: 203; g: 0; b: 0), (r: 155;
          g: 0; b: 0), (r: 107; g: 0; b: 0), (r: 59; g: 0; b: 0), (r: 247;
          g: 255; b: 0), (r: 215; g: 163; b: 0), (r: 175; g: 87; b: 0), (r: 135;
          g: 31; b: 0), (r: 95; g: 0; b: 0), (r: 255; g: 255; b: 255), (r: 223;
          g: 231; b: 255), (r: 195; g: 215; b: 255), (r: 163; g: 195; b: 255),
          (r: 135; g: 175; b: 255), (r: 255; g: 171; b: 0), (r: 255; g: 159;
          b: 0), (r: 255; g: 147; b: 0), (r: 255; g: 131; b: 0), (r: 255;
          g: 111; b: 0), (r: 255; g: 91; b: 0), (r: 255; g: 71; b: 0), (r: 255;
          g: 51; b: 0), (r: 255; g: 35; b: 0), (r: 255; g: 15; b: 0), (r: 0;
          g: 0; b: 255), (r: 253; g: 253; b: 253), (r: 247; g: 247; b: 247),
          (r: 239; g: 239; b: 239), (r: 227; g: 227; b: 227), (r: 219; g: 219;
          b: 219), (r: 211; g: 211; b: 211), (r: 203; g: 203; b: 203), (r: 195;
          g: 195; b: 195), (r: 187; g: 187; b: 187), (r: 179; g: 179; b: 179),
          (r: 171; g: 171; b: 171), (r: 163; g: 163; b: 163), (r: 155; g: 155;
          b: 155), (r: 147; g: 147; b: 147), (r: 139; g: 139; b: 139), (r: 131;
          g: 131; b: 131), (r: 123; g: 123; b: 123), (r: 115; g: 115; b: 115),
          (r: 107; g: 107; b: 107), (r: 99; g: 99; b: 99), (r: 87; g: 87;
          b: 87), (r: 79; g: 79; b: 79), (r: 71; g: 71; b: 71), (r: 63; g: 63;
          b: 63), (r: 55; g: 55; b: 55), (r: 47; g: 47; b: 47), (r: 39; g: 39;
          b: 39), (r: 31; g: 31; b: 31), (r: 23; g: 23; b: 23), (r: 15; g: 15;
          b: 15), (r: 7; g: 7; b: 7), (r: 0; g: 0; b: 0), (r: 191; g: 199;
          b: 223), (r: 183; g: 191; b: 215), (r: 179; g: 183; b: 207), (r: 171;
          g: 179; b: 203), (r: 163; g: 171; b: 195), (r: 159; g: 163; b: 187),
          (r: 151; g: 159; b: 183), (r: 147; g: 151; b: 175), (r: 139; g: 143;
          b: 167), (r: 135; g: 139; b: 163), (r: 127; g: 131; b: 155), (r: 119;
          g: 123; b: 147), (r: 115; g: 119; b: 139), (r: 107; g: 111; b: 135),
          (r: 103; g: 107; b: 127), (r: 95; g: 99; b: 119), (r: 91; g: 95;
          b: 115), (r: 87; g: 87; b: 107), (r: 79; g: 83; b: 99), (r: 75; g: 75;
          b: 95), (r: 67; g: 71; b: 87), (r: 63; g: 63; b: 79), (r: 55; g: 59;
          b: 75), (r: 51; g: 51; b: 67), (r: 47; g: 47; b: 59), (r: 39; g: 43;
          b: 55), (r: 35; g: 35; b: 47), (r: 31; g: 31; b: 39), (r: 23; g: 27;
          b: 35), (r: 19; g: 19; b: 27), (r: 15; g: 15; b: 19), (r: 11; g: 11;
          b: 15), (r: 255; g: 207; b: 179), (r: 231; g: 175; b: 143), (r: 207;
          g: 143; b: 111), (r: 183; g: 119; b: 87), (r: 159; g: 91; b: 63),
          (r: 135; g: 71; b: 43), (r: 111; g: 51; b: 27), (r: 87; g: 35; b: 15),
          (r: 255; g: 255; b: 0), (r: 227; g: 195; b: 0), (r: 199; g: 143;
          b: 0), (r: 171; g: 99; b: 0), (r: 147; g: 63; b: 0), (r: 119; g: 31;
          b: 0), (r: 91; g: 11; b: 0), (r: 67; g: 0; b: 0), (r: 223; g: 255;
          b: 167), (r: 207; g: 239; b: 135), (r: 191; g: 223; b: 103), (r: 179;
          g: 207; b: 75), (r: 167; g: 191; b: 51), (r: 159; g: 175; b: 31),
          (r: 151; g: 159; b: 11), (r: 143; g: 147; b: 0), (r: 199; g: 99;
          b: 31), (r: 183; g: 87; b: 23), (r: 171; g: 75; b: 19), (r: 155;
          g: 63; b: 11), (r: 143; g: 55; b: 7), (r: 127; g: 47; b: 7), (r: 115;
          g: 39; b: 0), (r: 103; g: 31; b: 0), (r: 251; g: 0; b: 0), (r: 227;
          g: 0; b: 0), (r: 199; g: 0; b: 0), (r: 171; g: 0; b: 0), (r: 143;
          g: 0; b: 0), (r: 115; g: 0; b: 0), (r: 87; g: 0; b: 0), (r: 57; g: 0;
          b: 0), (r: 127; g: 163; b: 199), (r: 95; g: 127; b: 171), (r: 67;
          g: 95; b: 147), (r: 43; g: 67; b: 123), (r: 23; g: 39; b: 95), (r: 11;
          g: 19; b: 71), (r: 0; g: 7; b: 47), (r: 0; g: 0; b: 23), (r: 195;
          g: 115; b: 71), (r: 183; g: 107; b: 63), (r: 175; g: 99; b: 59),
          (r: 163; g: 91; b: 51), (r: 155; g: 87; b: 47), (r: 147; g: 79;
          b: 43), (r: 135; g: 71; b: 35), (r: 127; g: 67; b: 31), (r: 115;
          g: 59; b: 27), (r: 107; g: 55; b: 23), (r: 99; g: 47; b: 19), (r: 87;
          g: 43; b: 15), (r: 79; g: 39; b: 15), (r: 67; g: 31; b: 11), (r: 59;
          g: 27; b: 7), (r: 51; g: 23; b: 7), (r: 255; g: 231; b: 179), (r: 239;
          g: 211; b: 155), (r: 223; g: 195; b: 135), (r: 211; g: 179; b: 119),
          (r: 195; g: 163; b: 99), (r: 183; g: 147; b: 83), (r: 167; g: 135;
          b: 71), (r: 151; g: 119; b: 55), (r: 139; g: 103; b: 43), (r: 123;
          g: 91; b: 31), (r: 111; g: 79; b: 23), (r: 95; g: 67; b: 15), (r: 79;
          g: 55; b: 11), (r: 67; g: 43; b: 7), (r: 51; g: 31; b: 0), (r: 39;
          g: 23; b: 0), (r: 131; g: 231; b: 103), (r: 115; g: 207; b: 83),
          (r: 99; g: 183; b: 67), (r: 83; g: 159; b: 55), (r: 71; g: 139;
          b: 43), (r: 59; g: 115; b: 31), (r: 47; g: 91; b: 23), (r: 35; g: 71;
          b: 15), (r: 255; g: 167; b: 255), (r: 223; g: 127; b: 231), (r: 195;
          g: 95; b: 207), (r: 163; g: 67; b: 183), (r: 135; g: 43; b: 159),
          (r: 107; g: 23; b: 135), (r: 79; g: 7; b: 111), (r: 55; g: 0; b: 91),
          (r: 255; g: 0; b: 255), (r: 255; g: 0; b: 255), (r: 255; g: 0;
          b: 255), (r: 255; g: 0; b: 255), (r: 255; g: 0; b: 255), (r: 255;
          g: 0; b: 255), (r: 255; g: 0; b: 255), (r: 255; g: 0; b: 255),
          (r: 255; g: 0; b: 255), (r: 255; g: 0; b: 255), (r: 255; g: 0;
          b: 255), (r: 255; g: 0; b: 255), (r: 255; g: 0; b: 255), (r: 255;
          g: 0; b: 255), (r: 255; g: 0; b: 255), (r: 255; g: 0; b: 255),
          (r: 255; g: 0; b: 255), (r: 255; g: 0; b: 255), (r: 255; g: 0;
          b: 255), (r: 255; g: 0; b: 255), (r: 255; g: 0; b: 255), (r: 255;
          g: 0; b: 255), (r: 255; g: 0; b: 255), (r: 255; g: 0; b: 255),
          (r: 255; g: 0; b: 255), (r: 255; g: 0; b: 255), (r: 255; g: 0;
          b: 255), (r: 255; g: 0; b: 255), (r: 255; g: 0; b: 255), (r: 255;
          g: 0; b: 255), (r: 255; g: 0; b: 255), (r: 255; g: 0; b: 255),
          (r: 255; g: 0; b: 255), (r: 255; g: 0; b: 255), (r: 255; g: 0;
          b: 255), (r: 255; g: 0; b: 255), (r: 255; g: 0; b: 255), (r: 255;
          g: 0; b: 255), (r: 255; g: 0; b: 255), (r: 255; g: 0; b: 255),
          (r: 255; g: 0; b: 255), (r: 255; g: 0; b: 255), (r: 255; g: 0;
          b: 255), (r: 255; g: 0; b: 255), (r: 255; g: 0; b: 255), (r: 255;
          g: 0; b: 255), (r: 255; g: 0; b: 255), (r: 255; g: 0; b: 255),
          (r: 255; g: 0; b: 255), (r: 255; g: 0; b: 255), (r: 255; g: 0;
          b: 255), (r: 255; g: 0; b: 255), (r: 255; g: 0; b: 255), (r: 255;
          g: 0; b: 255), (r: 255; g: 0; b: 255), (r: 255; g: 0; b: 255),
          (r: 255; g: 0; b: 255), (r: 255; g: 0; b: 255), (r: 255; g: 0;
          b: 255), (r: 255; g: 0; b: 255), (r: 255; g: 0; b: 255), (r: 255;
          g: 0; b: 255), (r: 255; g: 0; b: 255), (r: 255; g: 255; b: 255));

implementation

type
  TPCXPal = array [0 .. 255] of record r, g, b: byte;
end;

Function ApplyCMP(const cmpname: string; var pal: TCMPPal): boolean;
{ applies CMP to the palette }
var
  ct: TCMPTable;
begin
  result := false;
  if not LoadCMPTable(cmpname, ct) then
    exit;
  ApplyCMPTable(pal, ct);
  result := true;
end;

Function LoadCMPTable(const cmpname: string; var cmp: TCMPTable): boolean;
var
  f: TFile;
begin
  result := true;
  try
    f := OpenGameFile(cmpname);
    f.Fseek(Sizeof(TCMPHeader) + Sizeof(TCMPPal) + 63 * Sizeof(TCMPTable));
    f.Fread(cmp, Sizeof(cmp));
    f.Fclose;
  except
    on Exception do
      result := false;
  end;
end;

Procedure ApplyCMPTable(var pal: TCMPPal; const cmp: TCMPTable);
var
  i: Integer;
  pnew: TCMPPal;
begin
  for i := 0 to 255 do
    pnew[i] := pal[cmp[i]];
  pal := pnew;
end;

Function LoadCMPPal(const cmpname: string; var pal: TCMPPal): boolean;
{ loads palette from CMP }
var
  f: TFile;
begin
  result := true;
  try
    f := OpenGameFile(cmpname);
    f.Fseek(Sizeof(TCMPHeader));
    f.Fread(pal, Sizeof(pal));
    f.Fclose;
  except
    On Exception do
    begin
      pal := defCmppal;
      result := false;
    end;
  end;
end;

Procedure TMAT.SetPal(cmppal: TCMPPal);
var
  i: Integer;
begin
  for i := 0 to 255 do
    With cmppal[i], pal[i] do
    begin
      rgbRed := r;
      rgbGreen := g;
      rgbBlue := b;
    end;
end;

Destructor TMAT.Destroy;
begin
  if f <> nil then
    f.Fclose;
  Inherited Destroy;
end;

Constructor TMAT.Create(aF: TFile; nCell: Integer);
var
  i, j: Integer;
  msize, w, h: Integer;
  mh: TMatHeader;
begin
  Inherited Create;
  f := aF;

  f.Fread(mh, Sizeof(mh));
  if mh.tag <> 'MAT ' then
    Raise Exception.Create('Not a MAT file');

  if mh.version <> $32 then
    Raise Exception.Create('Invalid MAT file version');

  FInfo.ci := mh.ci;
  celCount := mh.celCount;
  if mh.mat_Type = 0 then
    begin
      iscolor := true;
      f.Fread(ch, Sizeof(ch));
      FInfo.width  := 64;
      FInfo.height := 64;
      colornum     := ch.colornum;
    end
  else
    begin
      if (mh.ci.bpp mod 8) <> 0 then
        Raise Exception.Create('MAT bitmap BPP not multiple of 8');

      if mh.ci.bpp > 32 then
        Raise Exception.Create('Unsupported MAT bitmap BPP');

      iscolor  := false;
      f.Fseek(Sizeof(mh) + mh.celCount * Sizeof(th));

      if nCell >= mh.celCount then
        nCurCell := mh.celCount - 1
      else
        nCurCell := nCell;

      for i := 0 to nCurCell do
      begin
        f.Fread(td, Sizeof(td));
        FInfo.width  := td.SizeX;
        FInfo.height := td.SizeY;
        w := td.SizeX;
        h := td.SizeY;
        msize := 0;
        for j := 0 to td.NumMipMaps - 1 do
        begin
          msize := msize + w * mh.ci.PixelSize * h;
          w := w div 2;
          h := h div 2;
        end;

        txoffs := f.fpos;
        f.Fseek(txoffs + msize);
      end;

      isAnimated := (celCount > 1) and (not iscolor);

      f.Fseek(txoffs);
    end;
end;

Procedure TMAT.LoadBits(var buf);
begin
  if iscolor then
  begin
    FillChar(buf, FInfo.width * FInfo.height, colornum);
    exit;
  end;
  f.Fread(buf, FInfo.height * Stride);
end;

Function TMAT.Stride: Uint32;
begin
  Result := FInfo.width * FInfo.ci.PixelSize;
end;

procedure TMAT.ReadRow(var buf);
begin
  if iscolor then
    begin
      FillChar(buf, FInfo.width, colornum);
      exit;
    end;
  f.Fread(buf, Stride);
end;

Function getColorMask(bpc: Uint32): Uint32; inline;
begin
    Result := $FFFFFFFF shr (32 - bpc);
end;

Function addGamma(c: Uint32; gamma: double):Uint32; inline;
begin
 Result := min(Round(c * gamma), 255);
end;

Function EncodePixel(r, g, b, a : Uint32 ; dstci: TColorInfo; gamma: double = 1.0): UInt32; inline;
begin
  Result := ((addGamma(r, gamma) shr dstci.redShr) shl dstci.redShl) or
            ((addGamma(g, gamma) shr dstci.greenShr) shl dstci.greenShl) or
            ((addGamma(b, gamma) shr dstci.blueShr) shl dstci.blueShl);

  if dstci.alphaBPP > 0 then
    Result := Result or ((a shr dstci.alphaShr) shl dstci.alphaShl);
end;

Function RecodePixel(pixel: Uint32; pixci: TColorInfo; dstci: TColorInfo; gamma: double = 1.0): Uint32;
begin
  if (pixci = dstci) and (gamma = 1.0) then
    begin
      Result := pixel;
      exit;
    end;

  const r: Uint32 = ((pixel shr pixci.redShl)   and getColorMask(pixci.redBPP))   shl pixci.redShr;
  const g: Uint32 = ((pixel shr pixci.greenShl) and getColorMask(pixci.greenBPP)) shl pixci.greenShr;
  const b: Uint32 = ((pixel shr pixci.blueShl)  and getColorMask(pixci.blueBPP))  shl pixci.blueShr;
  var a: Uint32 := 255;

  if pixci.alphaBPP <> 0 then
  begin
    a := ((pixel shr pixci.alphaShl) and getColorMask(pixci.alphaBPP)) shl pixci.alphaShr;
    if pixci.alphaBPP = 1 then
    begin
      a := Uint32(a > 0) * 255;
    end;
  end;

  Result := EncodePixel(r,g,b,a, dstci, gamma);
end;


procedure TMAT.RecodeTextureRow(srcRow: PByte; dstRow: PByte; dstci: TColorInfo; gamma : double);
begin
  if FInfo.ci.mode = Indexed then
    raise Exception.Create('Can''t read decoded row of texture with indexed color mode');

  const pixSize  = FInfo.ci.PixelSize;
  const hasAlpha = FInfo.ci.mode = RGBA;
  const pixSizeDst = dstci.PixelSize;
  for var sc := 0 to self.FInfo.width - 1 do
  begin
    var epix: Uint32 := 0;
    case pixSize of
      2: epix := Uint32(PWord(srcRow)^);
      3: epix := srcRow^ or ((srcRow + 1)^ shl 8) or ((srcRow + 2)^ shl 16);
      else epix := PUint32(srcRow)^;
    end;
    const dpix = RecodePixel(epix, FInfo.ci, dstci, gamma);
    var pdp: PByte := dstRow + sc * pixSizeDst;
    Move(PByte(@dpix)^, pdp^, pixSizeDst);
    inc(srcRow, pixSize);
  end;
end;

Procedure TMAT.ReadImage(pDestBuf: Pointer; dstci: TColorInfo; pitch: LongInt; const ppal:PTCMPPal; gamma: double = 1.0);
begin
  if FInfo.ci.mode <> Indexed then
    begin
       ReadImage(pDestBuf, dstci, pitch, gamma);
       exit;
    end;

  // Load palletized image
  if ppal = nil then
    raise Exception.Create('Cannot read index color texture, pallete is nil');

  var pRow: PByte;
  const pixSizeDst = dstci.PixelSize;
  GetMem(pRow, Stride);
  try
    for var y:=0 to FInfo.height-1 do
      begin
        ReadRow(pRow^);
        for var x:=0 to FInfo.width-1 do
          begin
            var c:=(pRow+x)^;
            With ppal[ord(c)] do
              begin
                const pix  = EncodePixel(r, g, b, 255, dstci, gamma);
                const pdst = PByte(pDestBuf) + y * pitch + x * pixSizeDst;
                Move(PByte(@pix)^, pdst^, pixSizeDst);
              end;
          end;
      end;
  finally
    FreeMem(prow);
  end;
end;

Procedure TMAT.ReadImage(pDestBuf: Pointer; const dstci: TColorInfo; const pitch: LongInt; const gamma: double = 1.0); // Reads texture from file, decodes it to RGB(A
begin
  if FInfo.ci.mode = Indexed then
    raise Exception.Create('Can''t read texture with indexed color mode');

  var pRow: PByte;
  GetMem(pRow, Stride);
  try
    for var r := 0 to FInfo.height -1 do
      begin
        ReadRow(pRow^);
        RecodeTextureRow(pRow, PByte(pDestBuf) + r * pitch, dstci, gamma);
      end;
  finally
    FreeMem(prow);
  end;
end;

Destructor TPCX.Destroy;
begin
  if f <> nil then
    f.Fclose;
  Inherited Destroy;
end;

Constructor TPCX.Create(aF: TFile);
var
  Pxpal: TPCXPal;
  ptype: byte;
  i: Integer;
begin
  Inherited Create;
  f := aF;

  f.Fread(ph, Sizeof(ph));
  if (ph.nplanes <> 1) or (ph.bitperpixel <> 8) then
  begin
    Raise Exception.Create('Not a 256 color PCX');
  end;
  FInfo.width := ph.x2 - ph.x1 + 1;
  FInfo.height := ph.y2 - ph.y1 + 1;
  FInfo.ci.mode := Indexed;
  FInfo.ci.bpp  := 8;

  f.Fseek(f.Fsize - Sizeof(Pxpal) - Sizeof(ptype));
  f.Fread(ptype, Sizeof(ptype));
  f.Fread(Pxpal, Sizeof(Pxpal));

  case ptype of
    10:
      for i := 0 to 255 do
        with pal[i] do
        begin
          rgbRed := Pxpal[i].r * 4;
          rgbGreen := Pxpal[i].g * 4;
          rgbBlue := Pxpal[i].b * 4;
          rgbReserved := 0;
        end;
    12:
      for i := 0 to 255 do
        with pal[i] do
        begin
          rgbRed := Pxpal[i].r;
          rgbGreen := Pxpal[i].g;
          rgbBlue := Pxpal[i].b;
          rgbReserved := 0;
        end;
  else
    Raise Exception.Create('Cannot locate palette');
  end;
  f.Fseek(Sizeof(ph));
  bpos := 0;
  bsize := 0;
end;

procedure TPCX.ReadRow(var buf);
  function GetNextByte: byte;
  begin
    if bpos >= bsize then
    begin
      bsize := f.Fsize - f.fpos;
      if bsize > Sizeof(lbuf) then
        bsize := Sizeof(lbuf);
      f.Fread(lbuf, bsize);
      bpos := 0;
    end;
    result := lbuf[bpos];
    inc(bpos);
  end;

var
  cw, bw: Integer;
  c, b1: byte;
  pbuf: ^byte;
begin
  bw := ph.byteperline;
  pbuf := @buf;
  cw := 0;
  while cw < bw do
  begin
    c := GetNextByte;
    if (c and $C0) <> $C0 then
    begin
      pbuf^ := c;
      inc(pbuf);
      inc(cw);
    end
    else
    begin
      b1 := GetNextByte;
      c := c and 63;
      FillChar(pbuf^, c, b1);
      inc(pbuf, c);
      inc(cw, c);
    end;
  end;
  if cw <> bw then
    PanMessage(mt_Warning, 'Warning, bad data in the PCX file');
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
