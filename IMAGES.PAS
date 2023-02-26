unit Images;

interface

uses Windows, Graphics, SysUtils, Classes;

Type
  TStoredAs = (ByLines, ByCols, byLines16, byLines24, byLines32);

  TImageInfo = class
    StoredAs: TStoredAs;
    width, height: word;
  end;

  TImageSource = class
  Protected
    FInfo: TImageInfo;
  Public
    Pal: Array [0 .. 255] of TRGBQuad;
    Property Info: TImageInfo read FInfo;
    Function LoadBitmap(bw, bh: Integer): TBitmap;
    procedure GetLine(var buf); virtual; abstract;
    Procedure GetCol(var buf); virtual; abstract;
  Protected
    Constructor Create;
    Destructor Destroy; override;
  Private
    Function LoadByLines(w, h: Integer): TBitmap;
    Function LoadByCols(w, h: Integer): TBitmap;
    Procedure WriteHeader(f: TStream);
    Function LoadByLines16(w, h: Integer): TBitmap;
    Procedure Dest;
  end;

implementation

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
  case Info.StoredAs of
    ByLines:
      Result := LoadByLines(bw, bh);
    ByCols:
      Result := LoadByCols(bw, bh);
    byLines16:
      Result := LoadByLines16(bw, bh);
  end;
end;

Procedure TImageSource.WriteHeader(f: TStream);
var
  Bi: TBitmapInfoHeader;
  Bfh: TBitmapFileHeader;
  bw, bh, bw4: Integer;
begin
  bw := Info.width;
  bh := Info.height;
  if bw and 3 = 0 then
    bw4 := bw
  else
    bw4 := bw and $FFFFFFFC + 4;

  With Bfh do
  begin
    bfType := $4D42; { 'BM' }
    bfOffBits := sizeof(Bfh) + sizeof(Bi) + sizeof(TRGBQuad) * 256;
    bfReserved1 := 0;
    bfReserved2 := 0;
    bfSize := bfOffBits + bh * bw4;
  end;

  FillChar(Bi, sizeof(Bi), 0);

  With Bi do
  begin
    biSize := sizeof(Bi);
    biWidth := bw;
    biHeight := bh;
    biPlanes := 1;
    biBitCount := 8;
  end;
  f.Write(Bfh, sizeof(Bfh));
  f.Write(Bi, sizeof(Bi));
  f.Write(Pal, sizeof(Pal));
end;

Function TImageSource.LoadByLines(w, h: Integer): TBitmap;
var
  i: Integer;
  Ms: TMemoryStream;
  pLine: PAnsiChar;
  pos: Longint;
  bw, bh, bw4: Integer;
begin
  Result := nil;
  bw := Info.width;
  bh := Info.height;
  if bw and 3 = 0 then
    bw4 := bw
  else
    bw4 := bw and $FFFFFFFC + 4;
  GetMem(pLine, bw4);

  Ms := TMemoryStream.Create;
  WriteHeader(Ms);
  Try
    pos := Ms.Position;
    for i := bh - 1 downto 0 do
    begin
      GetLine(pLine^);
      Ms.Position := pos + i * bw4;
      Ms.Write(pLine^, bw4);
    end;
    Ms.Position := 0;
    Result := TBitmap.Create;
    Result.LoadFromStream(Ms);
    Ms.Free;

  finally
    FreeMem(pLine);
  end;
end;

Function TImageSource.LoadByCols(w, h: Integer): TBitmap;
Const
  HeaderSize = sizeof(TBitmapInfoHeader) + sizeof(TBitmapFileHeader) + 256 *
    sizeof(TRGBQuad);
var
  i, j: Integer;
  Ms: TMemoryStream;
  pCol, pc: PAnsiChar;
  pos: Longint;
  bw, bh, bw4: Integer;
  pbits, pb: PAnsiChar;
begin
  Result := nil;
  bw := Info.width;
  bh := Info.height;
  if bw and 3 = 0 then
    bw4 := bw
  else
    bw4 := bw and $FFFFFFFC + 4;
  GetMem(pCol, bh);
  Ms := TMemoryStream.Create;
  Ms.SetSize(HeaderSize + bw4 * bh);
  WriteHeader(Ms);
  Try
    pos := Ms.Position;
    pbits := Ms.Memory;
    pbits := pbits + pos;
    for i := 0 to bw - 1 do
    begin
      GetCol(pCol^);
      pc := pCol;
      pb := pbits + i;
      for j := 0 to bh - 1 do
      begin
        pb^ := pc^;
        inc(pc);
        inc(pb, bw4);
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

Function TImageSource.LoadByLines16(w, h: Integer): TBitmap;
type
  TRGB = array [1 .. 3] of Longint;
var
  i: Integer;
  pLine: PAnsiChar;
  bw, bh, bw4: Integer;
  hbm: HBITMAP;
  bits: pointer;
  pbi: ^TBITMAPINFO;
  rgb: ^TRGB;

begin
  Result := nil;

  bw := Info.width;
  bh := Info.height;
  if bw * 2 and 3 = 0 then
    bw4 := bw * 2
  else
    bw4 := bw * 2 and $FFFFFFFC + 4;

  GetMem(pbi, sizeof(TBitmapInfoHeader) + 12);
  FillChar(pbi^, sizeof(TBitmapInfoHeader) + 12, 0);

  With pbi^.bmiHeader do
  begin
    biSize := sizeof(TBitmapInfoHeader);
    biWidth := bw;
    biHeight := bh;
    biPlanes := 1;
    biBitCount := 16;
    biCompression := BI_BITFIELDS;
    biSizeImage := bh * bw4;
  end;
  rgb := @pbi^.bmiColors;
  rgb^[3] := $1F;
  rgb^[2] := $7E0;
  rgb^[1] := $F800;

  Result := TBitmap.Create;

  hbm := CreateDIBSection(Result.Canvas.Handle, pbi^, DIB_RGB_COLORS,
    bits, 0, 0);

  Try
    for i := bh - 1 downto 0 do
    begin
      pLine := PAnsiChar(bits) + i * bw4;
      GetLine(pLine^);
    end;
    Result.Handle := hbm;
  finally
    FreeMem(pbi);
  end;
end;

end.
