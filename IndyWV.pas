unit IndyWV;

interface
uses System.Classes, System.SysUtils;

function IsWVSound(const istream: TMemoryStream): Boolean;
procedure ConvertWMSoundToWAV(const istream: TMemoryStream; out ostream: TMemoryStream); // Converts raw IndyWV file bytes to WAV raw bytes
implementation
uses MMSystem;
type
 TWVSM = array[0..3] of AnsiChar;
 TIndyWV = array[0..5] of AnsiChar;
 PTWVSM = ^TWVSM;
 TIndyWVHeader = packed record
    tag: TIndyWV;
    sampleRate: UInt32;
    sampleBitSize: UInt32;
    numChannels: UInt32;
    dataSize: UInt32;
    dataOffset: Int32;
  end;
  PIndyWVHeader = ^TIndyWVHeader;  
  TVWCompressorState = record
    unknown1: Byte;
    unknown2: Int16;
    unknown3: Byte;
    unknown4: Int16;
  end; 
 
  TWavFmt = record
    tag: UInt32;
    size: UInt32;
    audioFormat: UInt16;
    numChannels: UInt16;
    sampleRate: UInt32;
    byteRate: UInt32;
    blockAlign: UInt16;
    sampleBitSize: UInt16;
  end;
  TWavHeader = record
    tag: UInt32;
    size: UInt32;
    format: UInt32;
    fmt: TWavFmt;
  end;
  TWavDataChunkHeader = record
    tag: UInt32;
    size: UInt32;
  end;  
const
  kWVSM: TWVSM         = ('W', 'V', 'S', 'M');
  kIndyWV: TIndyWV     = ('I', 'N', 'D', 'Y', 'W', 'V');
  kLPCM: UInt16        = 1;
  
  kRiffChunkId: UInt32 = $46464952; // 'RIFF' in big-endian format
  kWavFormatId: UInt32 = $45564157; // 'WAVE' in big-endian format
  kFmtChunkId: UInt32  = $20746D66; // 'fmt ' in big-endian format
  kDataChunkId: UInt32 = $61746164; // 'data' in big-endian format

function Swap16(x: UInt16): UInt16; overload;
begin
  Result := UInt16(((Int16(x) and $FF00) shr 8) or ((Int16(x) and $00FF) shl 8));
end;

procedure WVSMInflateBlock(const istream: TMemoryStream; blockSize: Integer; var dest: TBytes; destOffset: Integer);
var
  nSamples: Integer;
  se: Byte;
  sel, ser: Integer;
  compressedSize: UInt16;

  function InflateSample(expander: Integer): UInt16;
  begin
    var bval: Byte;
    istream.ReadBuffer(bval, 1); // Read just a byte
    if (bval = $80) then
      begin
        istream.ReadBuffer(Result, SizeOf(Result));
        Result := Swap16(Result);
      end
    else
      Result := UInt16((Int8(bval) shl expander) and $FFFF);
  end;

  procedure InflateChannelSample(var i: Integer; const expander: Integer) ;
  begin
    var s: UInt16 := InflateSample(expander);
    Move(s, dest[destOffset + i * SizeOf(s)], SizeOf(s));
    Inc(i);
  end;  
  
begin
  nSamples := blockSize div 2;
  if (nSamples = 0) then
    Exit;

  istream.ReadBuffer(compressedSize, SizeOf(compressedSize));
  compressedSize := Swap16(compressedSize); // Note, big endian
  
  istream.ReadBuffer(se, SizeOf(se)); // sample expander
  sel := se shr 4;
  ser := se and $F;  
  
  var i := 0;
  while i < (nSamples - 1) do  
  begin
    InflateChannelSample(i, sel);     
    if i >= nSamples then Exit;
    InflateChannelSample(i, ser); 
  end;
end;
function WMInflate(const istream: TMemoryStream): TBytes;
var
  numChannels: Integer;
  state: TVWCompressorState;
  infSize, i: Integer;
  se: Int8;
  val: UInt16;
begin
  numChannels := 1;
  FillChar(state, SizeOf(state), 0);

  // Read infSize, unknown1, and unknown2
  istream.ReadBuffer(infSize, SizeOf(infSize));
  istream.ReadBuffer(se, SizeOf(se));
  istream.ReadBuffer(state.unknown2, SizeOf(state.unknown2));

  if (se < 0) then
    begin
      state.unknown1 := not se;
      numChannels := 2;
    end;

  state.unknown2 := Swap16(state.unknown2);
  if (numChannels > 1) then
  begin
    istream.ReadBuffer(state.unknown3, SizeOf(state.unknown3));
    istream.ReadBuffer(state.unknown4, SizeOf(state.unknown4));
    state.unknown4 := Swap16(state.unknown4);
  end;

  Result := nil;
  SetLength(Result, infSize);

  if (numChannels    = 2)     and
     (state.unknown2 = $1111) and
     (state.unknown3 = $64)   and
     (state.unknown4 = $2222) and
     (PTWVSM(PAnsiChar(istream.Memory) + istream.Position)^ = kWVSM) then
  begin
    istream.Seek(Length(kWVSM), soCurrent); // Skip kWVSM
    
    // Inflate n blocks of compressed sound data
    const blockSize: Integer = 4096;
    for i := 0 to (infSize div blockSize) - 1 do
      WVSMInflateBlock(istream, blockSize, Result, i * blockSize);

    // Read the remaining data, shorter than 1 block
    WVSMInflateBlock(istream, infSize mod blockSize, Result, infSize - infSize mod blockSize);
  end
  else
    raise Exception.Create('WMInflate: Cannot inflate sound data, unknown compression mode');
end;

function IsWVSound(const istream: TMemoryStream): Boolean;
  var tag: TIndyWV;
begin
  var curPos := istream.Position;
  try
    istream.ReadBuffer(tag, SizeOf(tag));
    Result := tag = kIndyWV;
  finally
    istream.Position := curPos;
  end;
end;

procedure ConvertWMSoundToWAV(const istream: TMemoryStream; out ostream: TMemoryStream);
var
  wvHeader: TIndyWVHeader;
  wavHeader: TWavHeader;
  sndData: TBytes;
  dataChunk: TWavDataChunkHeader;
begin

  // Read WV file
  istream.ReadBuffer(wvHeader, SizeOf(wvHeader));
  if (wvHeader.tag <> kIndyWV) then
    raise Exception.Create('ConvertWMSoundToWAV: Not IndyWV sound file');

  if wvHeader.dataOffset <> 0 then
    istream.Seek(wvHeader.dataOffset, soCurrent);

  sndData := WMInflate(istream);
  if Length(sndData) = 0 then
    begin
      ostream.Clear;
      Exit;
    end;

  dataChunk.tag  := kDataChunkId;
  dataChunk.size := Length(sndData);

  FillChar(wavHeader, SizeOf(wavHeader), 0);  
  wavHeader.tag               := kRiffChunkId;
  wavHeader.size              := 36 + dataChunk.size; // 36 = sizeof(WavHeader) + sizeof riff header;
  wavHeader.format            := kWavFormatId;
  wavHeader.fmt.tag           := kFmtChunkId;
  wavHeader.fmt.size          := 16;
  wavHeader.fmt.audioFormat   := kLPCM;
  wavHeader.fmt.numChannels   := wvHeader.numChannels;
  wavHeader.fmt.sampleRate    := wvHeader.sampleRate;
  wavHeader.fmt.sampleBitSize := wvHeader.sampleBitSize;
  wavHeader.fmt.blockAlign    := wvHeader.numChannels * wvHeader.sampleBitSize div 8;
  wavHeader.fmt.byteRate      := wvHeader.sampleRate  * wavHeader.fmt.blockAlign; 

  // Don't move this in case istream = ostream
  if ostream = nil then
    ostream := TMemoryStream.Create
  else
    ostream.Clear;     
  
  try
    ostream.WriteBuffer(wavHeader, SizeOf(wavHeader));
    ostream.WriteBuffer(dataChunk, SizeOf(dataChunk));
    ostream.WriteBuffer(sndData[0], Length(sndData));
    ostream.Position := 0;
  except
    ostream.Clear;
  end;
end;

end.

