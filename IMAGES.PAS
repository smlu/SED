unit Images;

interface

uses Windows, Graphics, SysUtils, Classes;

Type
  //TStoredAs = (Lines8, Cols8, RGB);

  {$MinEnumSize 4}
  TColorMode = (
     Indexed     = 0,
     RGB         = 1,
     RGBA        = 2,
     IndexedCols = 3
  );
  {$MinEnumSize 1} // Restore the default.

  TColorInfo = record
      mode: TColorMode;
      bpp: UInt32;  // Color bit depth per pixel

      redBPP: UInt32;   // red color component bits per pixel
      greenBPP: UInt32; // green color component bits per pixel
      blueBPP: UInt32; // blue color component bits per pixel

      redShl: UInt32;   // red color component encoding left bit shift
      greenShl: UInt32; // green color component encoding left bit shift
      blueShl: UInt32 ; // blue color component encoding left bit shift

      redShr: UInt32;   // red color component encoding right bit shift
      greenShr: UInt32; // green color component encoding right bit shift
      blueShr: UInt32;  // blue color component encoding right bit shift

      alphaBPP: UInt32; // alpha color component bits per pixel
      alphaShl: UInt32; // alpha color component encoding left bit shift
      alphaShr: UInt32; // alpha color component encoding right bit shift
      Function PixelSize: Uint32; // returns bpp in bytes i.e.: bpp / 8
      class operator Equal(a, b: TColorInfo): Boolean;
      class operator NotEqual(a, b: TColorInfo): Boolean;
  end;

const
  Indexed8: TColorInfo = (mode: Indexed; bpp:8;
    redBPP:0;   greenBPP:0; blueBPP:0;
    redShl:0;  greenShl:0; blueShl:0;
    redShr:0;   greenShr:0; blueShr:0;
    alphaBPP:0; alphaShl:0; alphaShr:0);

  IndexedCols8: TColorInfo = (mode: IndexedCols; bpp:8;
    redBPP:0;   greenBPP:0; blueBPP:0;
    redShl:0;  greenShl:0; blueShl:0;
    redShr:0;   greenShr:0; blueShr:0;
    alphaBPP:0; alphaShl:0; alphaShr:0);

  RGB555: TColorInfo = (mode: RGB; bpp:16;
    redBPP:5;   greenBPP:5; blueBPP:5;
    redShl:10;  greenShl:5; blueShl:0;
    redShr:3;   greenShr:3; blueShr:3;
    alphaBPP:0; alphaShl:0; alphaShr:0);

  RGBA4444: TColorInfo = (mode: RGB; bpp:16;
    redBPP:4;   greenBPP:4; blueBPP:4;
    redShl:12;  greenShl:8; blueShl:4;
    redShr:4;   greenShr:4; blueShr:4;
    alphaBPP:4; alphaShl:0; alphaShr:4);

  RGB24: TColorInfo = (mode: RGB; bpp:24;
    redBPP:8;   greenBPP:8; blueBPP:8;
    redShl:16;  greenShl:8; blueShl:0;
    redShr:0;   greenShr:0; blueShr:0;
    alphaBPP:0; alphaShl:0; alphaShr:0);

  BGR24: TColorInfo = (mode: RGB; bpp:24;
    redBPP:8;   greenBPP:8; blueBPP:8;
    redShl:0;   greenShl:8; blueShl:16;
    redShr:0;   greenShr:0; blueShr:0;
    alphaBPP:0; alphaShl:0; alphaShr:0);

  RGB32: TColorInfo = (mode: RGB; bpp:32;
    redBPP:8;   greenBPP:8; blueBPP:8;
    redShl:16;  greenShl:8; blueShl:0;
    redShr:0;   greenShr:0; blueShr:0;
    alphaBPP:0; alphaShl:0; alphaShr:0);

  BGR32: TColorInfo = (mode: RGB; bpp:32;
    redBPP:8;   greenBPP:8; blueBPP:8;
    redShl:0;   greenShl:8; blueShl:16;
    redShr:0;   greenShr:0; blueShr:0;
    alphaBPP:0; alphaShl:0; alphaShr:0);

  RGBA32: TColorInfo = (mode: RGBA; bpp:32;
    redBPP:8;   greenBPP:8;  blueBPP:8;
    redShl:24;  greenShl:16; blueShl:0;
    redShr:0;   greenShr:0;  blueShr:0;
    alphaBPP:8; alphaShl:0;  alphaShr:0);

  ABGR32: TColorInfo = (mode: RGBA; bpp:32;
    redBPP:8;   greenBPP:8;  blueBPP:8;
    redShl:0;   greenShl:8;  blueShl:16;
    redShr:0;   greenShr:0;  blueShr:0;
    alphaBPP:8; alphaShl:24; alphaShr:0);

  ARGB32: TColorInfo = (mode: RGBA; bpp:32;
    redBPP:8;   greenBPP:8;  blueBPP:8;
    redShl:16;  greenShl:8;  blueShl:0;
    redShr:0;   greenShr:0;  blueShr:0;
    alphaBPP:8; alphaShl:24; alphaShr:0);

  BGRA32: TColorInfo = (mode: RGBA; bpp:32;
    redBPP:8;   greenBPP:8;  blueBPP:8;
    redShl:8;   greenShl:16; blueShl:24;
    redShr:0;   greenShr:0;  blueShr:0;
    alphaBPP:8; alphaShl:0;  alphaShr:0);

type
  TImageInfo = class
    ci: TColorInfo;
    width, height: word;
    Function IsIndexed ():Boolean;
  end;

  TCMPPal = array [0 .. 255] of record r, g, b: byte;
  end;
  PTCMPPal = ^TCMPPal;

  TImageSource = class
  Protected
    FInfo: TImageInfo;
  Public
    Pal: Array [0 .. 255] of TRGBQuad;
    Property Info: TImageInfo read FInfo;
    Function LoadBitmap(bw, bh: Integer): TBitmap;
    procedure ReadRow(var buf); virtual; abstract;
    Procedure ReadCol(var buf); virtual; abstract;
    Procedure ReadImage(pDestBuf: Pointer; dstci: TColorInfo; pitch: LongInt; const ppal:PTCMPPal; gamma: double = 1.0);overload;virtual; abstract;
    Procedure ReadImage(pDestBuf: Pointer; const dstci: TColorInfo; const pitch: LongInt; const gamma: double = 1.0); overload;virtual; abstract; // Reads RGB(A) texture from file, encodes to dstci and writes it to buffer

  Protected
    Constructor Create;
    Destructor Destroy; override;
  Private
    //Function LoadByLines(w, h: Integer): TBitmap;
    //Function CreateBitmap(ci: TColorInfo; var pixels: pointer; var stride: integer): TBitmap;
    Function LoadByCols(w, h: Integer): TBitmap;
    Function WriteHeader(f: TStream; ci: TColorInfo): DWORD;
    Function LoadRgb(w, h: Integer): TBitmap;
    Procedure Dest;
  end;

implementation

Function TColorInfo.PixelSize: Uint32;
begin
  Result :=  Round(bpp / 8);
end;

class operator TColorInfo.Equal(a, b: TColorInfo): Boolean;
begin
  Result := (a.mode = b.mode)           and
            (a.bpp  = b.bpp)            and

            (a.redBPP = b.redBPP)       and
            (a.redShl = b.redShl)       and
            (a.redShr = b.redShr)       and

            (a.greenBPP = b.greenBPP)   and
            (a.greenShl = b.greenShl)   and
            (a.greenShr = b.greenShr)   and

            (a.blueBPP = b.blueBPP)     and
            (a.blueShl = b.blueShl)     and
            (a.blueShr = b.blueShr)     and

            (a.alphaBPP = b.alphaBPP)   and
            (a.alphaShl = b.alphaShl)   and
            (a.alphaShr = b.alphaShr);
end;

class operator TColorInfo.NotEqual(a, b: TColorInfo): Boolean;
begin
  Result := not (a = b);
end;

Function TImageInfo.IsIndexed(): Boolean;
begin
  Result := (ci.mode = Indexed) or (ci.mode = IndexedCols);
end;

Constructor TImageSource.Create;
begin
  FInfo := TImageInfo.Create;
end;

Procedure TImageSource.Dest;
begin
  FInfo.Free;
end;

Destructor TImageSource.Destroy;
begin
  Dest;
end;

Function TImageSource.LoadBitmap(bw, bh: Integer): TBitmap;
begin
  if (bw = -1) then
    bw := Info.width;
  if (bh = -1) then
    bh := Info.height;
  case Info.ci.mode of
    //Indexed:
    //  Result := LoadByLines(bw, bh);
    IndexedCols:
      Result := LoadByCols(bw, bh);
    else // RGB,RGBA
      Result := LoadRgb(bw, bh);
  end;
end;

Function getColorMask(bpc, cshl: integer): DWORD;
begin
    Result := DWORD(((1 shl bpc) -1) shl cshl);
end;

//Function TImageSource.CreateBitmap(ci: TColorInfo; var pixels: pointer; var stride: integer): TBitmap;
//const
//  MaxRGBQuads = MaxInt div SizeOf(TRGBQuad) - 1;
//
//type
//  TRGBQuadArray = array[0..MaxRGBQuads] of TRGBQuad;
//  PRGBQuadArray = ^TRGBQuadArray;
////type
//  //TRGB = array [1 .. 3] of Longint;
//type PTAGBITMAPINFO = ^tagBITMAPINFO;
//var
//  //i: Integer;
//  pLine: PAnsiChar;
//  hbm: HBITMAP;
//  bits: pointer;
//  //pbi: ^TBITMAPINFO;
//  //rgb: ^TRGB;
//
//begin
//  Result := nil;
//  const bw = Info.width;
//  const bh = Info.height;
//
//  const isIndexed = (ci.mode = Indexed) or (ci.mode = IndexedCols);
//  stride := ((((bw * ci.bpp) + 31) and (not 31)) shr 3);
//
////  if bw * 2 and 3 = 0 then
////    bw4 := bw * 2
////  else
////    bw4 := bw * 2 and $FFFFFFFC + 4;
//
//  //GetMem(pbi, sizeof(TBitmapInfoHeader) + 12);
//  //FillChar(pbi^, sizeof(TBitmapInfoHeader) + 12, 0);
//  //GetMem(pbi, sizeof(BITMAPV4HEADER));
//  //FillChar(pbi^, sizeof(BITMAPV4HEADER), 0);
//
//  var biHeader: BITMAPV4HEADER;
//  FillChar(biHeader, sizeof(BITMAPV4HEADER), 0);
//
//  With biHeader do
//  begin
//    bV4Size      := sizeof(biHeader);
//    bV4Width     := bw;
//    bV4Height    := -bh;
//    bV4Planes    := 1;
//    bV4BitCount  := ci.bpp;
//    bV4SizeImage := bh * stride;
//    bV4CSType    := $73524742; //LCS_sRGB
//    bV4V4Compression := BI_RGB;
//
//    if not isIndexed then
//    begin
//      if (ci <> RGB555) and (ci <> RGB24) and (ci <> ARGB32) then
//        begin
//          bV4V4Compression := BI_BITFIELDS;
//          bV4RedMask   := getColorMask(ci.redBPP, ci.redShl);
//          bV4GreenMask := getColorMask(ci.greenBPP, ci.greenShl);
//          bV4BlueMask  := getColorMask(ci.blueBPP, ci.blueShl);
//          bV4AlphaMask := getColorMask(ci.alphaBPP, ci.alphaShl);
//        end;
//    end;
//  end;
////  rgb := @pbi^.bmiColors;
////  rgb^[1] := getColorMask(info.ci.redBPP, info.ci.redShl);//$1F;
////  rgb^[2] := getColorMask(info.ci.greenBPP, info.ci.greenShl);//$7E0;
////  rgb^[3] := getColorMask(info.ci.blueBPP, info.ci.blueShl);//$F800;
//
//  Result := TBitmap.Create;
//  //Result.PixelFormat := pf32bit;
//  if ci.mode = RGBA then
//    begin
//      Result.Transparent := true;
//      Result.AlphaFormat := afDefined;
//    end;
//
//  var usage := DIB_RGB_COLORS;
////  if isIndexed then
////    begin
////      usage := DIB_PAL_COLORS;
////      var plp: PLOGPALETTE;
////      GetMem(plp, sizeof (LOGPALETTE) + 255 * sizeof(PALETTEENTRY));
////      plp.palVersion := $0300;
////      plp.palNumEntries := 256;
////      for var i := 0 to 256 -1   do
////        begin
////          {$R-}
////          plp.palPalEntry[i].peRed := Pal[i].rgbRed;
////          plp.palPalEntry[i].peGreen := Pal[i].rgbGreen;
////          plp.palPalEntry[i].peBlue := Pal[i].rgbBlue;
////          plp.palPalEntry[i].peFlags := 0;
////        end;
////
////      Result.Palette := CreatePalette(plp);
////      FreeMem(plp);
////    end;
//
//  hbm := CreateDIBSection(Result.Canvas.Handle,
//  (*pbi^*) (PTAGBITMAPINFO (@biHeader))^, DIB_RGB_COLORS, pixels, 0, 0);
//  if hbm = 0 then
//    RaiseLastOSError;
//
//  Result.HandleType := bmDIB;
//  Result.Handle := hbm;
//end;

Function TImageSource.WriteHeader(f: TStream; ci: TColorInfo): DWORD;
var
  Bi: TBitmapV4Header;
  Bfh: TBitmapFileHeader;
  bw, bh: Integer;
begin
  bw := Info.width;
  bh := Info.height;
  const stride:DWORD = ((((bw * ci.bpp) + 31) and (not 31)) shr 3);
  var palSize := 0;
  if (ci.mode = Indexed) or (ci.mode = IndexedCols) then
    palSize := sizeof(TRGBQuad) * 256;

  With Bfh do
  begin
    bfType := $4D42; { 'BM' }
    bfOffBits := sizeof(Bfh) + sizeof(Bi) + palSize;
    bfReserved1 := 0;
    bfReserved2 := 0;
    bfSize := bfOffBits + bh * stride;
  end;

  FillChar(Bi, sizeof(Bi), 0);

  With Bi do
  begin
    bV4Size      := sizeof(Bi);
    bV4Width     := bw;
    bV4Height    := -bh;
    bV4Planes    := 1;
    bV4BitCount  := ci.bpp;
    bV4SizeImage := bh * stride;
    bV4CSType    := $73524742; //LCS_sRGB
    bV4V4Compression := BI_RGB;

    if palSize = 0 then
    begin
      if (ci <> RGB555) and (ci <> RGB24) and (ci <> ARGB32) then
        begin
          bV4V4Compression := BI_BITFIELDS;
          bV4RedMask   := getColorMask(ci.redBPP, ci.redShl);
          bV4GreenMask := getColorMask(ci.greenBPP, ci.greenShl);
          bV4BlueMask  := getColorMask(ci.blueBPP, ci.blueShl);
          bV4AlphaMask := getColorMask(ci.alphaBPP, ci.alphaShl);
        end;
    end;
  end;
  f.Write(Bfh, sizeof(Bfh));
  f.Write(Bi, sizeof(Bi));
  if palSize > 0 then
    f.Write(Pal, sizeof(Pal));
  Result := stride;
end;

//Function TImageSource.LoadByLines(w, h: Integer): TBitmap;
//var
//  i: Integer;
//  Ms: TMemoryStream;
//  pLine: PAnsiChar;
//  pos: Longint;
//  bw, bh: Integer;
//begin
//  Result := nil;
//  bw := Info.width;
//  bh := Info.height;
////  if bw and 3 = 0 then
////    bw4 := bw
////  else
////    bw4 := bw and $FFFFFFFC + 4;
//
//
//  Ms := TMemoryStream.Create;
//  const stride = WriteHeader(Ms);
//  GetMem(pLine, stride);
//  Try
//    pos := Ms.Position;
//    for i := bh - 1 downto 0 do
//    begin
//      ReadRow(pLine^);
//      Ms.Position := pos + i * stride;
//      Ms.Write(pLine^, stride);
//    end;
//    Ms.Position := 0;
//    Result := TBitmap.Create;
//    Result.LoadFromStream(Ms);
//    Ms.Free;
//
//  finally
//    FreeMem(pLine);
//  end;
//end;

Function TImageSource.LoadByCols(w, h: Integer): TBitmap;
Const
  HeaderSize = sizeof(TBitmapInfoHeader) + sizeof(TBitmapFileHeader) + 256 *
    sizeof(TRGBQuad);
var
  i, j: Integer;
  Ms: TMemoryStream;
  pCol, pc: PAnsiChar;
  pos: Longint;
  bw, bh: Integer;
  pbits, pb: PAnsiChar;
begin
  Result := nil;
  bw := Info.width;
  bh := Info.height;
//  if bw and 3 = 0 then
//    bw4 := bw
//  else
//    bw4 := bw and $FFFFFFFC + 4;
  GetMem(pCol, bh);
  Ms := TMemoryStream.Create;
  //Ms.SetSize(HeaderSize + stride * bh);
  const stride = WriteHeader(Ms, IndexedCols8);
  Ms.SetSize(Ms.Size + stride * bh);

  Try
    pos := Ms.Position;
    pbits := Ms.Memory;
    pbits := pbits + pos;
    for i := 0 to bw - 1 do
    begin
      ReadCol(pCol^);
      pc := pCol;
      pb := pbits + i;
      for j := 0 to bh - 1 do
      begin
        pb^ := pc^;
        inc(pc);
        inc(pb, stride);
      end;
    end;
    Ms.Position := 0;
    Result := TBitmap.Create;
    Result.LoadFromStream(Ms);
    Ms.Free;

  finally
    FreeMem(pCol);
  end;
end;

//Function TImageSource.LoadRgb(w, h: Integer): TBitmap;
//var
//  pLine: PAnsiChar;
//  pPixels: pointer;
//  stride: integer;
//begin
//  const bw = Info.width;
//  const bh = Info.height;
//  var ci := info.ci;//ARGB32;
//  if Info.ci.mode = RGBA then ci := ARGB32
//  else if Info.ci.mode = Indexed then ci := RGB24;
//
//  Result := CreateBitmap(ci, pPixels, stride);
//  ReadImage(pPixels, ci, stride);
////  for var i := bh - 1 downto 0 do
////    begin
////      pLine := PAnsiChar(pPixels) + i * stride;
////      ReadRow(pLine^);
////    end;
//end;

Function TImageSource.LoadRgb(w, h: Integer): TBitmap;
  var
    pLine: PAnsiChar;

  type pRGBQuadArray = ^TRGBQuadArray;
  TRGBQuadArray = array [0..0] OF TRGBQuad;
begin
  var ci := info.ci;//ARGB32;
  const bw = Info.width;
  const bh = Info.height;
  if Info.ci.mode = RGBA then ci := ARGB32;

  var Ms := TMemoryStream.Create;
  const stride = WriteHeader(Ms, ci);
  Ms.SetSize(Ms.Size + stride * bh);
  GetMem(pLine, stride);
  Try
    if ci.mode <> RGBA then
      begin
        var pos := Ms.Position;
        for var i := 0 to bh - 1 do
        begin
          //ReadRow(pLine^);
          ReadRow((PByte(Ms.Memory) + pos + i * stride)^);
          //Ms.Position := pos + i * stride;
          //Ms.Write(pLine^, stride);
        end
      end
    else
      begin
        ReadImage((PByte(Ms.Memory) + Ms.Position), ci, stride);
      end;




    //Ms.Write( pRawImage^,stride * bh);



    Result := TBitmap.Create;
    if Info.ci.mode = RGBA then
      begin
        //var tmp := TBitmap.Create;
        //tmp.Assign(Result);
        //Result.PixelFormat := pf32Bit;
        Result.AlphaFormat := afDefined;
        //Result.Transparent := true;
      end;
    Ms.Position := 0;
    Result.LoadFromStream(Ms);


//    var fs := TFileStream.Create('tst.bmp', fmOpenWrite or fmCreate );
//    ms.Position := 0;
//    fs.CopyFrom(ms, ms.size);
//    fs.free;

//    if Info.ci.mode = RGBA then
//      begin
//        //var tmp := TBitmap.Create;
//        //tmp.Assign(Result);
//        Result.AlphaFormat := afDefined;
//        Result.Transparent := true;
//        Ms.Position := 0;
//        Result.LoadFromStream(Ms);
//
//
////        for var nScanLineCount := 0 to  Result.Height - 1 do
////        begin
////          var pscanLine32:pRGBQuadArray := Result.Scanline[nScanLineCount];
////          var pscanLine32_src: pRGBQuadArray := tmp.ScanLine[nScanLineCount];
////          for var nPixelCount := 0 to Result.Width - 1 do
////            begin
////             {$R-}
////              pscanLine32[nPixelCount].rgbReserved := pscanLine32_src[nPixelCount].rgbReserved;
////              pscanLine32[nPixelCount].rgbBlue := pscanLine32_src[nPixelCount].rgbBlue;
////              pscanLine32[nPixelCount].rgbRed  := pscanLine32_src[nPixelCount].rgbRed;
////              pscanLine32[nPixelCount].rgbGreen:= pscanLine32_src[nPixelCount].rgbGreen;
////            end;
////        end;
//
//        //tmp.Free;
//      end;

  finally
    FreeMem(pLine);
    Ms.Free;
  End;
end;

//Function TImageSource.LoadRgb(w, h: Integer): TBitmap;
//const
//  MaxRGBQuads = MaxInt div SizeOf(TRGBQuad) - 1;
//
//type
//  TRGBQuadArray = array[0..MaxRGBQuads] of TRGBQuad;
//  PRGBQuadArray = ^TRGBQuadArray;
////type
//  //TRGB = array [1 .. 3] of Longint;
//type PTAGBITMAPINFO = ^tagBITMAPINFO;
//var
//  i: Integer;
//  pLine: PAnsiChar;
//  hbm: HBITMAP;
//  bits: pointer;
//  //pbi: ^TBITMAPINFO;
//  //rgb: ^TRGB;
//
//begin
//  Result := nil;
//  const bw = Info.width;
//  const bh = Info.height;
//
//  const stride:integer = ((((bw * info.ci.bpp) + 31) and (not 31)) shr 3);
//
////  if bw * 2 and 3 = 0 then
////    bw4 := bw * 2
////  else
////    bw4 := bw * 2 and $FFFFFFFC + 4;
//
//  //GetMem(pbi, sizeof(TBitmapInfoHeader) + 12);
//  //FillChar(pbi^, sizeof(TBitmapInfoHeader) + 12, 0);
//  //GetMem(pbi, sizeof(BITMAPV4HEADER));
//  //FillChar(pbi^, sizeof(BITMAPV4HEADER), 0);
//
//  var biHeader: BITMAPV4HEADER;
//  FillChar(biHeader, sizeof(BITMAPV4HEADER), 0);
//
//  With biHeader do
//  begin
////    biSize := sizeof(BITMAPV4HEADER);
////    biWidth := bw;
////    biHeight := bh;
////    biPlanes := 1;
////    biBitCount := 16;
////    biCompression := BI_BITFIELDS;
////    biSizeImage := bh * stride;
//    bV4Size      := sizeof(BITMAPV4HEADER);
//    bV4Width     := bw;
//    bV4Height    := bh;
//    bV4Planes    := 1;
//    bV4BitCount  := info.ci.bpp;
//    bV4SizeImage := bh * stride;
//    bV4CSType    := $73524742; //LCS_sRGB
//
//    if (info.ci = RGB555) or (info.ci = RGB24) then
//      bV4V4Compression := BI_RGB
//    else
//      begin
//        bV4V4Compression := BI_BITFIELDS;
//        bV4RedMask   := getColorMask(info.ci.redBPP, info.ci.redShl);
//        bV4GreenMask := getColorMask(info.ci.greenBPP, info.ci.greenShl);
//        bV4BlueMask  := getColorMask(info.ci.blueBPP, info.ci.blueShl);
//        bV4AlphaMask := 00;//getColorMask(info.ci.alphaBPP, info.ci.alphaBPP);
//      end;
//  end;
////  rgb := @pbi^.bmiColors;
////  rgb^[1] := getColorMask(info.ci.redBPP, info.ci.redShl);//$1F;
////  rgb^[2] := getColorMask(info.ci.greenBPP, info.ci.greenShl);//$7E0;
////  rgb^[3] := getColorMask(info.ci.blueBPP, info.ci.blueShl);//$F800;
//
//  Result := TBitmap.Create;
//  if info.ci.alphaBPP > 0 then
//    begin
//      Result.Transparent := true;
//      Result.AlphaFormat := afPremultiplied;
//    end;
//
//  hbm := CreateDIBSection(Result.Canvas.Handle,
//  (*pbi^*) (PTAGBITMAPINFO (@biHeader))^, DIB_RGB_COLORS, bits, 0, 0);
//  if hbm = 0 then
//    RaiseLastOSError;
//
//  Try
//    for i := bh - 1 downto 0 do
//    begin
//      pLine := PAnsiChar(bits) + i * stride;
//      ReadRow(pLine^);
////      for var j:= 0 to Round(stride/ 4) -1 do
////        var p :=PRGBQuad(pLine[j]).rgbReserved ;
//
//    end;
//    Result.HandleType := bmDIB;
//    Result.Handle := hbm;
//
//     for var Y := 0 to Result.Height - 1 do
//    begin
//      var Line: PRGBQuadArray := Result.ScanLine[Y];
//      for var X := 0 to Result.Width - 1 do
//      begin
//        Line[X].rgbBlue := Y;
//        Line[X].rgbGreen := X;
//        Line[X].rgbRed := 255 - X;
//        Line[X].rgbReserved := 30;
//      end;
//    end;
//  finally
//    //FreeMem(pbi);
//  end;
//end;

end.
