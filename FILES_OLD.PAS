unit Files;

{This unit defines abstract file class
 as well as implementation of simple
 disk file, buffered file and a text file}

interface
uses SysUtils;

Const
{OpenFileWrite flags}
     fm_Create=1; {Create new file}          
     fm_LetReWrite=2;{Let rewrite file
                    if exists - OpenFileWrite}
     fm_AskUser=4; {Ask user if something}
     fm_CreateAskRewrite=fm_Create+fm_LetRewrite+fm_AskUser;
{OpenFileRead&Write flags}
     fm_Share=8;   {Let share file}
     fm_Buffered=16;

{Constants for text files}
txt_bufsize=8096;
txt_maxlinesize=4096;
Type

 TFile=class
  Function GetFullName:String;virtual;abstract;
  Function GetShortName:String;virtual;abstract;
  Function Fread(var buf;size:integer):integer;virtual;abstract;
  Function Fwrite(var buf;size:integer):integer;virtual;abstract;
  Function Fsize:Longint;virtual;abstract;
  Function Fpos:Longint;virtual;abstract;
  Procedure Fseek(Pos:longint);virtual;abstract;
  Destructor FClose;virtual;abstract;
 end;

TBufFile=class(TFile)
 f:TFile;
 pbuf:pchar;
 pos:Longint;
 bpos,bsize:word;
 bcapacity:word;
 Constructor CreateRead(bf:TFile);
 Constructor CreateWrite(bf:TFile);
 Function GetFullName:String;override;
 Function GetShortName:String;override;
 Function Fread(var buf;size:integer):integer;override;
 Function Fwrite(var buf;size:integer):integer;override;
 Function Fsize:Longint;override;
 Function Fpos:Longint;override;
 Procedure Fseek(Pos:longint);override;
 Destructor FClose;override;
Private
 Procedure InitBuffer(size:word);
 Procedure ReadBuffer;
 Procedure WriteBuffer;
end;

TTextFile=Class
 f:TFile;
 bpos,bsize:word;
 buffer:array[0..txt_bufsize] of char;
 Constructor CreateRead(bf:TFile);
 Constructor CreateWrite(bf:TFile);
 Procedure Readln(var s:String);
 Procedure Writeln(s:String);
 Function Eof:Boolean;
 Destructor FClose;
Private
 Procedure LoadBuffer;
 Procedure SeekEoln;
end;

TDiskFile=class(TFile)
  f:File;
  Constructor CreateRead(path:TFileName);
  Constructor CreateWrite(path:TFileName);
  Function GetFullName:String;override;
  Function GetShortName:String;override;
  Function Fread(var buf;size:integer):integer;override;
  Function Fwrite(var buf;size:integer):integer;override;
  Function Fsize:Longint;override;
  Function Fpos:Longint;override;
  Procedure Fseek(Pos:longint);override;
  Destructor FClose;override;
end;

Function OpenFileRead(path:TFileName;mode:word):TFile;
Function OpenFileWrite(path:TFileName;mode:word):TFile;

implementation

Function OpenFileRead(path:TFileName;mode:word):TFile;
begin
 Result:=TDiskFile.CreateRead(path);
end;

Function OpenFileWrite(path:TFileName;mode:word):TFile;
begin
 Result:=TDiskFile.CreateWrite(path);
end;


Constructor TDiskFile.CreateRead(path:TFileName);
begin
 Assign(f,path);
 FileMode:=0;
 Reset(f,1);
end;

Constructor TDiskFile.CreateWrite(path:TFileName);
begin
 Assign(f,path);
 ReWrite(f,1);
end;


Function TDiskFile.GetFullName:String;
begin
 Result:=TFileRec(f).name;
end;

Function TDiskFile.GetShortName:String;
begin
 Result:=ExtractFileName(GetFullName);
end;


Function TDiskFile.Fread(var buf;size:integer):integer;
begin
 BlockRead(f,buf,size,Result);
end;

Function TDiskFile.Fwrite(var buf;size:integer):integer;
begin
 BlockWrite(f,buf,size,Result);
end;

Function TDiskFile.Fsize:Longint;
begin
 Result:=FileSize(f);
end;

Function TDiskFile.Fpos:Longint;
begin
 Result:=FilePos(f);
end;

Procedure TDiskFile.Fseek(Pos:longint);
begin
 Seek(f,Pos);
end;

Destructor TDiskFile.FClose;
begin
 CloseFile(f);
end;

{TTextFile methods} 

Destructor TTextFile.FClose;
begin
 f.FClose;
end;

Constructor TTextFile.CreateRead(bf:TFile);
begin
 f:=bf;
 f.Fseek(0);
 LoadBuffer;
end;

Constructor TTextFile.CreateWrite(bf:TFile);
begin
 f:=bf;
end;

Procedure TTextFile.SeekEoln;
var ps:Pchar;
begin
 ps:=StrScan(@buffer[bpos],#10);
 if ps<>nil then {EoLn found}
 begin
  bpos:=ps-@buffer+1;
  if bpos=bsize then LoadBuffer;
 end
 else {Eoln not found}
 begin
  Repeat
   LoadBuffer;
   if eof then exit;
   ps:=StrScan(buffer,#10);
  Until(ps<>nil);
  bpos:=ps-@buffer;
  SeekEoln;
 end;
end;

Procedure TTextFile.Readln(var s:String);
var ps,pend:Pchar;
    tmp:array[0..txt_maxlinesize-1] of char;
    ssize:word;
    l:word;
begin
 s:='';
 if bpos=bsize then LoadBuffer;
 if eof then exit;

 ps:=buffer+bpos;
 pend:=StrScan(ps,#10);
 if pend<>nil then
 begin
  ssize:=pend-ps;
  if ssize>txt_maxlinesize then ssize:=txt_maxlinesize; {Limit string size to 255 chars}
  s:=StrLCopy(tmp,ps,ssize);
  l:=Length(s);
  if l<>0 then if s[l]=#13 then SetLength(s,l-1);
  inc(bpos,ssize);
  SeekEoln;
 end else
 begin
  ssize:=bsize-bpos;
  if ssize>txt_maxlinesize then ssize:=txt_maxlinesize;
  s:=StrLCopy(tmp,ps,ssize); {copy the tail of the buffer}
  LoadBuffer;
  if eof then exit;
  pend:=StrScan(buffer,#10);
  if pend=nil then ssize:=bsize else ssize:=pend-@buffer;
  if ssize+length(s)>txt_maxlinesize then ssize:=txt_maxlinesize-length(s);
  s:=ConCat(s,StrLCopy(tmp,buffer,ssize));
  inc(bpos,ssize);
  l:=Length(s);
  if l<>0 then if s[l]=#13 then SetLength(s,l-1);
  SeekEoln;
 end;
end;

Procedure TTextFile.Writeln(s:String);
const
     eol:array[0..0] of char=#10;
begin
 f.FWrite(s[1],length(s));
 f.FWrite(eol,sizeof(eol));
end;

Function TTextFile.Eof:Boolean;
begin
 result:=bsize=0;
end;

Procedure TTextFile.LoadBuffer;
var bytes:longint;
begin
 bytes:=f.fsize-f.fpos;
 if bytes>txt_bufsize then bytes:=txt_bufsize;
 f.FRead(buffer,bytes);
 bpos:=0;
 bsize:=bytes;
 buffer[bsize]:=#0;
end;

{TBufFile methods}

Procedure TBufFile.InitBuffer(size:word);
begin
 bcapacity:=size;
 GetMem(pbuf,bcapacity);
end;

Procedure TBufFile.ReadBuffer;
begin
 f.Fseek(fpos);
{ f.Fread(pbuf^,bcapacity}
end;

Procedure TBufFile.WriteBuffer;
begin
end;

Constructor TBufFile.CreateRead(bf:TFile);
begin
 InitBuffer(2048);
 f:=bf;
 ReadBuffer;
end;

Constructor TBufFile.CreateWrite(bf:TFile);
begin
end;

Function TBufFile.GetFullName:String;
begin
 result:=f.GetFullName;
end;

Function TBufFile.GetShortName:String;
begin
 Result:=f.GetShortName;
end;

Function TBufFile.Fread(var buf;size:integer):integer;
begin
end;

Function TBufFile.Fwrite(var buf;size:integer):integer;
begin
end;

Function TBufFile.Fsize:Longint;
begin
end;

Function TBufFile.Fpos:Longint;
begin
end;

Procedure TBufFile.Fseek(Pos:longint);
begin
end;

Destructor TBufFile.FClose;
begin
 f.FClose;
 FreeMem(pbuf);
end;

end.
